#include "sony/core/DeviceService.h"
#include "sony/core/IpcServer.h"
#include "sony/transport/PlatformTransport.h"
#include "sony/transport/Logger.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

using namespace sony;
using namespace sony::core;
using namespace sony::transport;

namespace {

std::atomic<bool> g_stop{false};

void handleSignal(int) { g_stop = true; }

void printHelp() {
    std::cout
        << "sonyd — background daemon that owns the Bluetooth connection to Sony audio devices\n\n"
        << "Usage: sonyd [options]\n\n"
        << "Options:\n"
        << "  -s, --socket <path>        Custom Unix domain socket path\n"
        << "  -v, --verbose              Enable verbose diagnostic logging\n"
        << "  -h, --help                 Display this help menu\n\n"
        << "sony-wh-cli (and any other client) talks to this daemon over its local\n"
        << "IPC socket when it is running, so multiple callers can share a single\n"
        << "Bluetooth connection instead of fighting over it.\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string socketPath = defaultSocketPath();
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        } else if ((arg == "-s" || arg == "--socket") && i + 1 < argc) {
            socketPath = argv[++i];
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return 1;
        }
    }

    if (verbose) {
        Logger::setLogLevel(LogLevel::Debug);
        Logger::setDeveloperMode(true);
    }

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    std::shared_ptr<ITransport> transport = transport::createPlatformTransport();
    std::shared_ptr<IDeviceDiscovery> discovery = transport::createPlatformDiscovery();
    // No eager startAutoConnect() here: sonyd connects on demand when a
    // command needs the device (see IpcServer) and releases the Bluetooth
    // link again after a period of inactivity, so it doesn't permanently
    // block a phone's own companion app from connecting.
    auto service = std::make_shared<DeviceService>(transport, discovery);

    IpcServer server(service, socketPath);
    try {
        server.start();
    } catch (const std::exception& ex) {
        std::cerr << "Error: failed to start sonyd: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "sonyd listening on " << server.socketPath() << " (Ctrl+C to stop)\n";
    while (!g_stop && server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    server.stop();
    return 0;
}
