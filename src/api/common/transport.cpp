#include "transport.h"
#include "../protocol/colour_data_protocol.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <algorithm>

namespace Synesthesia::API {

#ifndef _WIN32
class UnixDomainSocketTransport::Impl {
public:
    explicit Impl(const std::string& socket_path, bool is_server)
        : socket_path_(socket_path), is_server_(is_server) {}
    
    ~Impl() {
        stop();
    }
    
    bool start() {
        if (running_.load()) return true;
        
        server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (server_fd_ == -1) return false;
        
        if (is_server_) {
            unlink(socket_path_.c_str());
            
            struct sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
            
            if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
                close(server_fd_);
                return false;
            }
            
            if (listen(server_fd_, 16) == -1) {
                close(server_fd_);
                return false;
            }
        } else {
            struct sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
            
            if (connect(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
                close(server_fd_);
                return false;
            }
        }
        
        running_.store(true);
        worker_thread_ = std::thread(&Impl::workerLoop, this);
        return true;
    }
    
    void stop() {
        if (!running_.load()) return;
        
        running_.store(false);
        
        if (server_fd_ != -1) {
            close(server_fd_);
            server_fd_ = -1;
        }
        
        for (auto& [id, fd] : client_sockets_) {
            close(fd);
        }
        client_sockets_.clear();
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        
        if (is_server_) {
            unlink(socket_path_.c_str());
        }
    }
    
    bool isRunning() const { return running_.load(); }
    
    bool sendMessage(std::span<const uint8_t> data, const std::string& target_id) {
        if (!running_.load()) return false;
        
        if (is_server_) {
            auto it = client_sockets_.find(target_id);
            if (it != client_sockets_.end()) {
                return sendToSocket(it->second, data);
            }
        } else {
            return sendToSocket(server_fd_, data);
        }
        return false;
    }
    
    bool broadcastMessage(std::span<const uint8_t> data) {
        if (!running_.load() || !is_server_) return false;
        
        bool success = true;
        for (const auto& [id, fd] : client_sockets_) {
            success &= sendToSocket(fd, data);
        }
        return success;
    }
    
    void setMessageCallback(MessageCallback callback) { message_callback_ = std::move(callback); }
    void setConnectionCallback(ConnectionCallback callback) { connection_callback_ = std::move(callback); }
    void setErrorCallback(ErrorCallback callback) { error_callback_ = std::move(callback); }
    
    std::string getEndpointInfo() const { return socket_path_; }
    
    std::vector<std::string> getConnectedClients() const {
        std::vector<std::string> clients;
        for (const auto& [id, fd] : client_sockets_) {
            clients.push_back(id);
        }
        return clients;
    }

private:
    void workerLoop() {
        if (is_server_) {
            serverLoop();
        } else {
            clientLoop();
        }
    }
    
    void serverLoop() {
        fd_set read_fds;
        int max_fd = server_fd_;
        
        while (running_.load()) {
            FD_ZERO(&read_fds);
            FD_SET(server_fd_, &read_fds);
            
            for (const auto& [id, fd] : client_sockets_) {
                FD_SET(fd, &read_fds);
                max_fd = std::max(max_fd, fd);
            }
            
            struct timeval timeout{0, 100000};
            int activity = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
            
            if (activity < 0) break;
            if (activity == 0) continue;
            
            if (FD_ISSET(server_fd_, &read_fds)) {
                acceptNewClient();
            }
            
            for (auto it = client_sockets_.begin(); it != client_sockets_.end();) {
                if (FD_ISSET(it->second, &read_fds)) {
                    if (!handleClientData(it->first, it->second)) {
                        if (connection_callback_) {
                            connection_callback_(it->first, false);
                        }
                        close(it->second);
                        client_buffers_.erase(it->first);
                        it = client_sockets_.erase(it);
                        continue;
                    }
                }
                ++it;
            }
        }
    }
    
    void clientLoop() {
        std::vector<uint8_t> message_buffer;
        message_buffer.reserve(4096);
        
        while (running_.load()) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server_fd_, &read_fds);
            
            struct timeval timeout{0, 100000};
            int activity = select(server_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
            
            if (activity < 0) break;
            if (activity == 0) continue;
            
            if (FD_ISSET(server_fd_, &read_fds)) {
                if (!receiveMessage(server_fd_, message_buffer, "server")) {
                    break;
                }
            }
        }
    }
    
    void acceptNewClient() {
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd != -1) {
            std::string client_id = "client_" + std::to_string(client_fd);
            client_sockets_[client_id] = client_fd;
            
            if (connection_callback_) {
                connection_callback_(client_id, true);
            }
        }
    }
    
    bool handleClientData(const std::string& client_id, int fd) {
        auto it = client_buffers_.find(client_id);
        if (it == client_buffers_.end()) {
            client_buffers_[client_id].reserve(4096);
            it = client_buffers_.find(client_id);
        }
        
        return receiveMessage(fd, it->second, client_id);
    }
    
    bool receiveMessage(int fd, std::vector<uint8_t>& buffer, const std::string& sender_id) {
        constexpr size_t header_size = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint64_t); // Full MessageHeader
        uint8_t temp_buffer[4096];
        
        ssize_t bytes = recv(fd, temp_buffer, sizeof(temp_buffer), 0);
        if (bytes <= 0) {
            return false;
        }
        
        buffer.insert(buffer.end(), temp_buffer, temp_buffer + bytes);
        
        while (buffer.size() >= header_size) {
            // Check magic number (first 4 bytes)
            uint32_t magic;
            std::memcpy(&magic, buffer.data(), sizeof(magic));
            if (magic != 0x53594E45) {
                buffer.clear();
                return true;
            }
            
            // Get message length (offset 6: magic(4) + version(1) + type(1))
            uint16_t length;
            std::memcpy(&length, buffer.data() + 6, sizeof(length));
            
            size_t total_message_size = header_size + length;
            if (buffer.size() < total_message_size) {
                break;
            }
            
            // Validate message size to prevent excessive memory usage
            if (total_message_size > MAX_MESSAGE_SIZE) {
                buffer.clear();
                return true;
            }
            
            if (message_callback_) {
                message_callback_(std::span<const uint8_t>(buffer.data(), total_message_size), sender_id);
            }
            
            buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(total_message_size));
        }
        
        return true;
    }
    
    bool sendToSocket(int fd, std::span<const uint8_t> data) {
        size_t total_sent = 0;
        while (total_sent < data.size()) {
            ssize_t sent = send(fd, data.data() + total_sent, data.size() - total_sent, MSG_NOSIGNAL);
            if (sent <= 0) return false;
            total_sent += static_cast<size_t>(sent);
        }
        return true;
    }
    
    std::string socket_path_;
    bool is_server_;
    std::atomic<bool> running_{false};
    
    int server_fd_{-1};
    std::unordered_map<std::string, int> client_sockets_;
    std::unordered_map<std::string, std::vector<uint8_t>> client_buffers_;
    
    std::thread worker_thread_;
    
    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    ErrorCallback error_callback_;
};

UnixDomainSocketTransport::UnixDomainSocketTransport(const std::string& socket_path, bool is_server)
    : pImpl(std::make_unique<Impl>(socket_path, is_server)) {}

UnixDomainSocketTransport::~UnixDomainSocketTransport() = default;

bool UnixDomainSocketTransport::start() { return pImpl->start(); }
void UnixDomainSocketTransport::stop() { pImpl->stop(); }
bool UnixDomainSocketTransport::isRunning() const { return pImpl->isRunning(); }
bool UnixDomainSocketTransport::sendMessage(std::span<const uint8_t> data, const std::string& target_id) { return pImpl->sendMessage(data, target_id); }
bool UnixDomainSocketTransport::broadcastMessage(std::span<const uint8_t> data) { return pImpl->broadcastMessage(data); }
void UnixDomainSocketTransport::setMessageCallback(MessageCallback callback) { pImpl->setMessageCallback(std::move(callback)); }
void UnixDomainSocketTransport::setConnectionCallback(ConnectionCallback callback) { pImpl->setConnectionCallback(std::move(callback)); }
void UnixDomainSocketTransport::setErrorCallback(ErrorCallback callback) { pImpl->setErrorCallback(std::move(callback)); }
std::string UnixDomainSocketTransport::getEndpointInfo() const { return pImpl->getEndpointInfo(); }
std::vector<std::string> UnixDomainSocketTransport::getConnectedClients() const { return pImpl->getConnectedClients(); }
#endif

std::unique_ptr<ITransport> TransportFactory::createTransport(const std::string& endpoint, bool is_server) {
#ifdef _WIN32
    return std::make_unique<NamedPipeTransport>(endpoint, is_server);
#else
    return std::make_unique<UnixDomainSocketTransport>(endpoint, is_server);
#endif
}

}