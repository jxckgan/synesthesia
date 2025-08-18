#include "transport.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <algorithm>
#include <condition_variable>

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
            unlink(socket_path_.c_str()); // Remove existing socket file
            
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
            
            struct timeval timeout{0, 100000}; // 100ms timeout
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
                        it = client_sockets_.erase(it);
                        continue;
                    }
                }
                ++it;
            }
        }
    }
    
    void clientLoop() {
        char buffer[4096];
        while (running_.load()) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server_fd_, &read_fds);
            
            struct timeval timeout{0, 100000}; // 100ms timeout
            int activity = select(server_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
            
            if (activity < 0) break;
            if (activity == 0) continue;
            
            if (FD_ISSET(server_fd_, &read_fds)) {
                ssize_t bytes = recv(server_fd_, buffer, sizeof(buffer), 0);
                if (bytes <= 0) {
                    break; // Connection lost
                }
                
                if (message_callback_) {
                    message_callback_(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buffer), bytes), "server");
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
        char buffer[4096];
        ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            return false; // Connection lost
        }
        
        if (message_callback_) {
            message_callback_(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buffer), bytes), client_id);
        }
        return true;
    }
    
    bool sendToSocket(int fd, std::span<const uint8_t> data) {
        size_t total_sent = 0;
        while (total_sent < data.size()) {
            ssize_t sent = send(fd, data.data() + total_sent, data.size() - total_sent, MSG_NOSIGNAL);
            if (sent <= 0) return false;
            total_sent += sent;
        }
        return true;
    }
    
    std::string socket_path_;
    bool is_server_;
    std::atomic<bool> running_{false};
    
    int server_fd_{-1};
    std::unordered_map<std::string, int> client_sockets_;
    
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

class UDPDiscoveryTransport::Impl {
public:
    explicit Impl(uint16_t port, bool is_server) : port_(port), is_server_(is_server) {}
    
    ~Impl() { stop(); }
    
    bool start() {
        if (running_.load()) return true;
        
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ == -1) return false;
        
        int broadcast = 1;
        setsockopt(socket_fd_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
        
        if (is_server_) {
            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port_);
            
            if (bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
                close(socket_fd_);
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
        
        if (socket_fd_ != -1) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    bool isRunning() const { return running_.load(); }
    
    bool sendMessage(std::span<const uint8_t> data, const std::string& target_id) {
        if (!running_.load()) return false;
        
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        
        if (target_id.empty() || target_id == "broadcast") {
            addr.sin_addr.s_addr = INADDR_BROADCAST;
        } else {
            inet_pton(AF_INET, target_id.c_str(), &addr.sin_addr);
        }
        
        return sendto(socket_fd_, data.data(), data.size(), 0, (struct sockaddr*)&addr, sizeof(addr)) > 0;
    }
    
    bool broadcastMessage(std::span<const uint8_t> data) {
        return sendMessage(data, "broadcast");
    }
    
    void setMessageCallback(MessageCallback callback) { message_callback_ = std::move(callback); }
    void setConnectionCallback(ConnectionCallback callback) { connection_callback_ = std::move(callback); }
    void setErrorCallback(ErrorCallback callback) { error_callback_ = std::move(callback); }
    
    std::string getEndpointInfo() const {
        return "UDP:" + std::to_string(port_);
    }
    
    std::vector<std::string> getConnectedClients() const {
        return {}; // UDP is connectionless
    }

private:
    void workerLoop() {
        char buffer[4096];
        struct sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        
        while (running_.load()) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(socket_fd_, &read_fds);
            
            struct timeval timeout{0, 100000}; // 100ms timeout
            int activity = select(socket_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
            
            if (activity <= 0) continue;
            
            if (FD_ISSET(socket_fd_, &read_fds)) {
                ssize_t bytes = recvfrom(socket_fd_, buffer, sizeof(buffer), 0,
                                       (struct sockaddr*)&client_addr, &addr_len);
                if (bytes > 0 && message_callback_) {
                    std::string sender_ip = inet_ntoa(client_addr.sin_addr);
                    message_callback_(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buffer), bytes), sender_ip);
                }
            }
        }
    }
    
    uint16_t port_;
    bool is_server_;
    std::atomic<bool> running_{false};
    
    int socket_fd_{-1};
    std::thread worker_thread_;
    
    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    ErrorCallback error_callback_;
};

UDPDiscoveryTransport::UDPDiscoveryTransport(uint16_t port, bool is_server)
    : pImpl(std::make_unique<Impl>(port, is_server)) {}

UDPDiscoveryTransport::~UDPDiscoveryTransport() = default;

bool UDPDiscoveryTransport::start() { return pImpl->start(); }
void UDPDiscoveryTransport::stop() { pImpl->stop(); }
bool UDPDiscoveryTransport::isRunning() const { return pImpl->isRunning(); }
bool UDPDiscoveryTransport::sendMessage(std::span<const uint8_t> data, const std::string& target_id) { return pImpl->sendMessage(data, target_id); }
bool UDPDiscoveryTransport::broadcastMessage(std::span<const uint8_t> data) { return pImpl->broadcastMessage(data); }
void UDPDiscoveryTransport::setMessageCallback(MessageCallback callback) { pImpl->setMessageCallback(std::move(callback)); }
void UDPDiscoveryTransport::setConnectionCallback(ConnectionCallback callback) { pImpl->setConnectionCallback(std::move(callback)); }
void UDPDiscoveryTransport::setErrorCallback(ErrorCallback callback) { pImpl->setErrorCallback(std::move(callback)); }
std::string UDPDiscoveryTransport::getEndpointInfo() const { return pImpl->getEndpointInfo(); }
std::vector<std::string> UDPDiscoveryTransport::getConnectedClients() const { return pImpl->getConnectedClients(); }

std::unique_ptr<ITransport> TransportFactory::createMainTransport(const std::string& endpoint, bool is_server) {
#ifdef _WIN32
    return std::make_unique<NamedPipeTransport>(endpoint, is_server);
#else
    return std::make_unique<UnixDomainSocketTransport>(endpoint, is_server);
#endif
}

std::unique_ptr<ITransport> TransportFactory::createDiscoveryTransport(uint16_t port, bool is_server) {
    return std::make_unique<UDPDiscoveryTransport>(port, is_server);
}

}