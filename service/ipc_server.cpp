/**
 * ipc_server.cpp — Phase 2
 */
#include "ipc_server.h"
#include <iostream>
#include <sstream>

namespace magicmouse {

IpcServer::IpcServer(IpcCommandHandler handler)
    : handler_(std::move(handler)) {}

IpcServer::~IpcServer() { stop(); }

void IpcServer::start() {
    running_ = true;
    accept_thread_ = std::thread([this] { acceptLoop(); });
}

void IpcServer::stop() {
    running_ = false;
    // Wake the accept loop by connecting a dummy client
    HANDLE dummy = CreateFile(kPipeName, GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (dummy != INVALID_HANDLE_VALUE) CloseHandle(dummy);
    if (accept_thread_.joinable()) accept_thread_.join();
}

void IpcServer::broadcast(const std::string& json_event) {
    std::string msg = json_event + "\n";
    std::lock_guard<std::mutex> lk(clients_mutex_);
    for (HANDLE h : clients_) {
        DWORD written = 0;
        WriteFile(h, msg.c_str(), static_cast<DWORD>(msg.size()), &written, nullptr);
    }
}

void IpcServer::acceptLoop() {
    while (running_) {
        // Create a named pipe instance
        HANDLE pipe = CreateNamedPipe(
            kPipeName,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096, 4096, 0, nullptr);

        if (pipe == INVALID_HANDLE_VALUE) {
            std::cerr << "[IPC] CreateNamedPipe failed: " << GetLastError() << "\n";
            break;
        }

        // ACL: restrict to current user + LocalSystem
        // TODO Phase 2: set DACL on pipe via SetSecurityInfo

        if (!ConnectNamedPipe(pipe, nullptr)) {
            DWORD err = GetLastError();
            if (err != ERROR_PIPE_CONNECTED) {
                CloseHandle(pipe);
                continue;
            }
        }

        {
            std::lock_guard<std::mutex> lk(clients_mutex_);
            clients_.push_back(pipe);
        }

        // Spawn a thread per connected client
        std::thread([this, pipe] { clientLoop(pipe); }).detach();
    }
}

void IpcServer::clientLoop(HANDLE pipe) {
    char buf[4096];
    std::string partial;

    while (running_) {
        DWORD read = 0;
        BOOL ok = ReadFile(pipe, buf, sizeof(buf) - 1, &read, nullptr);
        if (!ok || read == 0) break;

        buf[read] = '\0';
        partial += buf;

        // Process line-delimited JSON commands
        size_t pos;
        while ((pos = partial.find('\n')) != std::string::npos) {
            std::string cmd = partial.substr(0, pos);
            partial = partial.substr(pos + 1);
            if (cmd.empty()) continue;

            std::string response;
            if (handler_) {
                response = handler_(cmd) + "\n";
            }
            if (!response.empty()) {
                DWORD written = 0;
                WriteFile(pipe, response.c_str(), static_cast<DWORD>(response.size()),
                          &written, nullptr);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lk(clients_mutex_);
        clients_.erase(std::remove(clients_.begin(), clients_.end(), pipe), clients_.end());
    }
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

} // namespace magicmouse
