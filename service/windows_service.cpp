/**
 * windows_service.cpp — Phase 1
 *
 * Windows Service entry point for MagicMouseService.
 * Registers SCM handlers, starts the device manager pipeline,
 * and runs until the service is stopped.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <iostream>

#include "device_manager.h"
#include "power_manager.h"
#include "watchdog.h"

static SERVICE_STATUS          g_ServiceStatus = {};
static SERVICE_STATUS_HANDLE   g_hServiceStatus = nullptr;
static HANDLE                  g_hStopEvent = nullptr;

static magicmouse::DeviceManager* g_DeviceManager = nullptr;
static magicmouse::PowerManager*  g_PowerManager  = nullptr;
static magicmouse::Watchdog*      g_Watchdog       = nullptr;

//-----------------------------------------------------------------------------
// Service control handler
//-----------------------------------------------------------------------------

static VOID WINAPI ServiceCtrlHandler(DWORD ctrl) {
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_hServiceStatus, &g_ServiceStatus);
        SetEvent(g_hStopEvent);
        break;

    case SERVICE_CONTROL_SESSIONCHANGE:
        // Handled in DeviceManager::onSessionChange()
        if (g_DeviceManager) g_DeviceManager->onSessionChange();
        break;

    default:
        break;
    }
}

//-----------------------------------------------------------------------------
// ServiceMain
//-----------------------------------------------------------------------------

static VOID WINAPI ServiceMain(DWORD /*argc*/, LPSTR* /*argv*/) {
    g_hServiceStatus = RegisterServiceCtrlHandler(
        "MagicMouseService", ServiceCtrlHandler);
    if (!g_hServiceStatus) return;

    // Report start pending
    g_ServiceStatus.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState            = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted        = 0;
    g_ServiceStatus.dwWin32ExitCode           = NO_ERROR;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint              = 1;
    g_ServiceStatus.dwWaitHint               = 5000;
    SetServiceStatus(g_hServiceStatus, &g_ServiceStatus);

    // Create stop event
    g_hStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!g_hStopEvent) {
        g_ServiceStatus.dwCurrentState  = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        SetServiceStatus(g_hServiceStatus, &g_ServiceStatus);
        return;
    }

    // Start subsystems
    magicmouse::DeviceManager deviceManager;
    magicmouse::PowerManager  powerManager(&deviceManager);
    magicmouse::Watchdog      watchdog(&deviceManager);

    g_DeviceManager = &deviceManager;
    g_PowerManager  = &powerManager;
    g_Watchdog      = &watchdog;

    powerManager.start();
    watchdog.start();
    deviceManager.start();

    // Report running
    g_ServiceStatus.dwCurrentState    = SERVICE_RUNNING;
    g_ServiceStatus.dwControlsAccepted =
        SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE;
    g_ServiceStatus.dwCheckPoint      = 0;
    g_ServiceStatus.dwWaitHint       = 0;
    SetServiceStatus(g_hServiceStatus, &g_ServiceStatus);

    // Wait for stop signal
    WaitForSingleObject(g_hStopEvent, INFINITE);

    // Teardown
    deviceManager.stop();
    watchdog.stop();
    powerManager.stop();

    CloseHandle(g_hStopEvent);

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_hServiceStatus, &g_ServiceStatus);
}

//-----------------------------------------------------------------------------
// main — runs as service OR console (debug mode)
//-----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    // If launched with --console, run interactively for debug
    if (argc > 1 && std::string(argv[1]) == "--console") {
        std::cout << "[MagicMouseService] Running in console (debug) mode.\n";
        magicmouse::DeviceManager dm;
        magicmouse::Watchdog      wd(&dm);
        dm.start();
        wd.start();
        std::cout << "Press Enter to stop...\n";
        std::cin.get();
        dm.stop();
        wd.stop();
        return 0;
    }

    SERVICE_TABLE_ENTRY serviceTable[] = {
        { const_cast<LPSTR>("MagicMouseService"), ServiceMain },
        { nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcher(serviceTable)) {
        // If error 1063: not run as a service, suggest console mode
        DWORD err = GetLastError();
        if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            std::cerr << "Not running as a Windows service.\n"
                      << "Use --console flag for debug mode.\n";
        }
        return static_cast<int>(err);
    }

    return 0;
}
