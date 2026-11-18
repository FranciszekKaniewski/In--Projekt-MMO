#pragma once

#include "GameClient.h"
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

struct NetworkError : std::runtime_error {
    explicit NetworkError(const std::string& code) : std::runtime_error(code) {}
};

// Windows Schannel supplies TLS; no credentials are sent before CA/host checks.
class TlsConnection {
public:
    explicit TlsConnection(std::function<bool()> cancelled);
    ~TlsConnection();
    void connect(const ServerConfig& config);
    void sendFrame(const std::string& body, std::chrono::milliseconds timeout);
    std::optional<std::string> receiveFrame(std::chrono::milliseconds timeout, std::size_t maxBytes);
    void close(); // Worker thread only.
    void interrupt(); // Thread-safe cancellation of pending socket operations.

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
