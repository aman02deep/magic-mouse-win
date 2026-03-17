/**
 * ipc_server.h — Phase 2
 *
 * Named Pipe server running in the service (\\.\pipe\MagicMouseService).
 * Accepts connections from the UI process (user session), dispatches JSON commands,
 * and broadcasts events (battery, status, log) to all connected clients.
 */
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

namespace magicmouse {

/// Callback invoked when a JSON command arrives from the UI
using IpcCommandHandler = std::function<std::string(const std::string& json_cmd)>;

class IpcServer {
public:
    static constexpr const char* kPipeName = R"(\\.\pipe\MagicMouseService)";

    explicit IpcServer(IpcCommandHandler handler);
    ~IpcServer();

    void start();
    void stop();

    /// Broadcast a JSON event string to all connected UI clients
    void broadcast(const std::string& json_event);

private:
    void acceptLoop();
    void clientLoop(HANDLE pipe);

    IpcCommandHandler   handler_;
    std::thread         accept_thread_;
    std::atomic<bool>   running_{ false };

    std::mutex                    clients_mutex_;
    std::vector<HANDLE>           clients_;
};

} // namespace magicmouse
