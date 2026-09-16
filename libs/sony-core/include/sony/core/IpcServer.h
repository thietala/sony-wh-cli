#pragma once

#include "IDeviceService.h"
#include "IpcProtocol.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <future>
#include <functional>

namespace sony::core {

std::string defaultSocketPath();

class IpcServer {
public:
    // idleDisconnect: how long the device connection may sit unused (no
    // command executed) before sonyd releases it, so other apps (e.g. the
    // headphones' own phone app) can take over its exclusive control link.
    explicit IpcServer(std::shared_ptr<IDeviceService> service, std::string socketPath = defaultSocketPath(),
        std::chrono::milliseconds idleDisconnect = std::chrono::seconds(15));
    ~IpcServer();

    IpcServer(const IpcServer&) = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    void start();
    void stop() noexcept;
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] const std::string& socketPath() const noexcept;

private:
    void _serverLoop();
    void _executeLoop();
    void _connectOnDemand(const std::string& line);
    struct Job { std::string line; std::promise<std::string> result; std::atomic<bool> cancelled{false}; };

    std::shared_ptr<IDeviceService> _service;
    std::string _socketPath;
    std::atomic<bool> _running{false};
    int _listenFd{-1};
    std::thread _worker;
    std::thread _executor;
    std::mutex _queueMutex;
    std::condition_variable _queueCv;
    std::deque<std::shared_ptr<Job>> _jobs;
    int _lockFd{-1};
    unsigned long long _socketInode{0};
    std::chrono::milliseconds _idleDisconnect;
    std::chrono::steady_clock::time_point _lastActivity{std::chrono::steady_clock::now()};
};

} // namespace sony::core
