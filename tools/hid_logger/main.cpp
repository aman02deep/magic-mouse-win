/**
 * hid_logger/main.cpp
 *
 * Phase 0 tool: Reads raw HID packets from a connected Magic Mouse and:
 *   1. Prints timestamped hex to stdout
 *   2. Writes packets to a binary .bin file for offline analysis
 *
 * Usage:
 *   hid_logger.exe [--device <path>] [--output <filename.bin>]
 *   If no arguments, lists detected devices and prompts for selection.
 */

#include "device_detector.h"
#include "hid_reader.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using namespace magicmouse;

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

static std::string timestampedFilename() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm local_tm {};
#ifdef _WIN32
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "hid_log_%Y%m%d_%H%M%S.bin", &local_tm);
    return std::string(buf);
}

static std::string elapsedMs(const std::chrono::steady_clock::time_point& start) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    std::ostringstream oss;
    oss << std::setw(6) << (ms / 1000) << "."
        << std::setw(3) << std::setfill('0') << (ms % 1000);
    return oss.str();
}

static void printHex(const uint8_t* buf, int len) {
    for (int i = 0; i < len; ++i) {
        std::printf("%02X ", buf[i]);
        if ((i + 1) % 16 == 0 && i + 1 < len) std::printf("\n          ");
    }
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    std::cout << "=== Magic Mouse HID Logger (Phase 0) ===\n\n";

    // --- 1. Enumerate devices ---
    auto devices = enumerateMagicMice();

    if (devices.empty()) {
        std::cerr << "ERROR: No Magic Mouse detected.\n"
                  << "Make sure the Magic Mouse is paired and connected via Bluetooth,\n"
                  << "and that Apple Boot Camp drivers (or HID drivers) are installed.\n";
        return 1;
    }

    std::cout << "Detected Magic Mouse device(s):\n";
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];
        std::printf("  [%zu] %s   VID=%04X  PID=%04X  Serial=%s\n",
                    i, d.model_name.c_str(), d.vendor_id, d.product_id, d.serial.c_str());
        std::printf("       Path: %s\n", d.path.c_str());
    }

    // --- 2. Device selection ---
    size_t choice = 0;
    if (devices.size() > 1) {
        std::cout << "\nSelect device [0-" << (devices.size() - 1) << "]: ";
        std::cin >> choice;
        if (choice >= devices.size()) {
            std::cerr << "Invalid selection.\n";
            return 1;
        }
    } else {
        std::cout << "\nUsing device [0]\n";
    }

    const DeviceInfo& selected = devices[choice];

    // --- 3. Output file ---
    std::string outfile = timestampedFilename();
    // Allow --output override
    for (int i = 1; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], "--output") == 0) {
            outfile = argv[i + 1];
        }
    }

    std::ofstream binfile(outfile, std::ios::binary);
    if (!binfile.is_open()) {
        std::cerr << "ERROR: Could not open output file: " << outfile << "\n";
        return 1;
    }

    std::cout << "\nOpening device: " << selected.path << "\n";
    std::cout << "Logging to:     " << outfile << "\n";
    std::cout << "\nPress Ctrl+C to stop.\n";
    std::cout << std::string(70, '-') << "\n\n";

    // --- 4. Open and read ---
    HidReader reader(selected.path);
    if (!reader.isOpen()) {
        std::cerr << "ERROR: Could not open device. Try running as Administrator.\n";
        return 1;
    }

    auto start = std::chrono::steady_clock::now();
    uint8_t buf[HidReader::kReportBufferSize];
    long long packet_count = 0;

    // Write a simple binary header: magic bytes + metadata
    const char header[] = "MMHIDLOG\x01\x00"; // magic + version
    binfile.write(header, sizeof(header) - 1);
    // Store selected PID in header (2 bytes LE)
    uint8_t pid_bytes[2] = {
        static_cast<uint8_t>(selected.product_id & 0xFF),
        static_cast<uint8_t>((selected.product_id >> 8) & 0xFF)
    };
    binfile.write(reinterpret_cast<char*>(pid_bytes), 2);

    while (true) {
        int bytes_read = reader.read(buf, 100 /*ms timeout*/);

        if (bytes_read < 0) {
            std::cerr << "\nERROR reading from device: " << reader.lastError() << "\n";
            break;
        }
        if (bytes_read == 0) {
            // Timeout — normal, just no data this tick
            continue;
        }

        ++packet_count;

        // --- Print to stdout ---
        std::printf("[%s] #%-6lld (%2d bytes): ", elapsedMs(start).c_str(), packet_count, bytes_read);
        printHex(buf, bytes_read);
        std::printf("\n");
        std::fflush(stdout);

        // --- Write to binary file ---
        // Format per packet: [2-byte length LE][N bytes data]
        uint8_t len_bytes[2] = {
            static_cast<uint8_t>(bytes_read & 0xFF),
            static_cast<uint8_t>((bytes_read >> 8) & 0xFF)
        };
        binfile.write(reinterpret_cast<char*>(len_bytes), 2);
        binfile.write(reinterpret_cast<char*>(buf), bytes_read);
        binfile.flush();
    }

    std::cout << "\n\nLogged " << packet_count << " packets to " << outfile << "\n";
    std::cout << "Save this file as: docs/packet-samples/first-capture.bin\n";
    return 0;
}
