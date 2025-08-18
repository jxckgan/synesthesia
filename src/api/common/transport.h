#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <span>

namespace Synesthesia::API {

using MessageCallback = std::function<void(std::span<const uint8_t> data, const std::string& sender_id)>;
using ConnectionCallback = std::function<void(const std::string& client_id, bool connected)>;
using ErrorCallback = std::function<void(const std::string& error_message)>;

class ITransport {
public:
    virtual ~ITransport() = default;
    
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    
    virtual bool sendMessage(std::span<const uint8_t> data, const std::string& target_id = "") = 0;
    virtual bool broadcastMessage(std::span<const uint8_t> data) = 0;
    
    virtual void setMessageCallback(MessageCallback callback) = 0;
    virtual void setConnectionCallback(ConnectionCallback callback) = 0;
    virtual void setErrorCallback(ErrorCallback callback) = 0;
    
    virtual std::string getEndpointInfo() const = 0;
    virtual std::vector<std::string> getConnectedClients() const = 0;
};

#ifdef _WIN32
class NamedPipeTransport : public ITransport {
public:
    explicit NamedPipeTransport(const std::string& pipe_name, bool is_server = false);
    ~NamedPipeTransport() override;
    
    bool start() override;
    void stop() override;
    bool isRunning() const override;
    
    bool sendMessage(std::span<const uint8_t> data, const std::string& target_id = "") override;
    bool broadcastMessage(std::span<const uint8_t> data) override;
    
    void setMessageCallback(MessageCallback callback) override;
    void setConnectionCallback(ConnectionCallback callback) override;
    void setErrorCallback(ErrorCallback callback) override;
    
    std::string getEndpointInfo() const override;
    std::vector<std::string> getConnectedClients() const override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};
#else
class UnixDomainSocketTransport : public ITransport {
public:
    explicit UnixDomainSocketTransport(const std::string& socket_path, bool is_server = false);
    ~UnixDomainSocketTransport() override;
    
    bool start() override;
    void stop() override;
    bool isRunning() const override;
    
    bool sendMessage(std::span<const uint8_t> data, const std::string& target_id = "") override;
    bool broadcastMessage(std::span<const uint8_t> data) override;
    
    void setMessageCallback(MessageCallback callback) override;
    void setConnectionCallback(ConnectionCallback callback) override;
    void setErrorCallback(ErrorCallback callback) override;
    
    std::string getEndpointInfo() const override;
    std::vector<std::string> getConnectedClients() const override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};
#endif

class TransportFactory {
public:
    static std::unique_ptr<ITransport> createTransport(const std::string& endpoint, bool is_server);
};

}