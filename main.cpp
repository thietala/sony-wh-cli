#include "sony/core/DeviceService.h"
#include "sony/core/IpcClient.h"
#include "sony/core/IpcProtocol.h"
#include "sony/protocol/FrameCodec.h"
#include "sony/transport/FakeTransport.h"
#include "sony/transport/PlatformTransport.h"
#include "sony/transport/Logger.h"


#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace sony;
using namespace sony::core;
using namespace sony::protocol;
using namespace sony::transport;

namespace {

void printHelp() {
    std::cout << "sony-wh-cli — CLI diagnostic and control tool for Sony audio devices\n\n"
              << "Usage: sony-wh-cli [options] <command> [args...]\n\n"
              << "Commands:\n"
              << "  devices                    List discovered paired Sony devices\n"
              << "  info                       Display connected device information & capabilities\n"
              << "  battery                    Display battery percentage and charging state\n"
              << "  anc on|off                 Enable or disable Active Noise Cancelling\n"
              << "  ambient <1-20>|off         Set Ambient Sound level or turn ambient off\n"
              << "  eq get                     Display active Equalizer preset and band levels\n"
              << "  eq preset <name>           Set Equalizer preset (bright, bass-boost, vocal, etc.)\n"
              << "  eq custom <cb> <b1..b5>    Apply custom Clear Bass (-10..10) and 5 EQ bands\n"
              << "  eq <preset>                Shorthand for eq preset <preset>\n"
              << "  dsee on|off|auto           Toggle DSEE sound enhancement\n"
              << "  apo <0-5>                  Set Auto-Power-Off duration preset index\n"
              << "  status                     Display connection status\n\n"
              << "Options:\n"
              << "  -s, --socket <path>        Custom Unix domain socket path for sonyd\n"
              << "  --direct                   Bypass sonyd for a one-off direct Bluetooth\n"
              << "                             session (refuses to run alongside a live sonyd)\n"
              << "  -v, --verbose              Enable verbose diagnostic logging\n"
              << "  -h, --help                 Display this help menu\n\n"
              << "sony-wh-cli requires sonyd to be running; start it with `sonyd` first,\n"
              << "or pass --direct to skip the daemon for a single command.\n\n"
              << "Examples:\n"
              << "  sony-wh-cli anc on\n"
              << "  sony-wh-cli ambient 10\n"
              << "  sony-wh-cli eq bass-boost\n"
              << "  sony-wh-cli dsee on\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string socketPath = defaultSocketPath();
    bool direct = false;
    bool verbose = false;
    std::vector<std::string> commandTokens;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        } else if ((arg == "-s" || arg == "--socket") && i + 1 < argc) {
            socketPath = argv[++i];
        } else if (arg == "--direct") {
            direct = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else {
            commandTokens.push_back(std::move(arg));
        }
    }

    if (commandTokens.empty()) {
        printHelp();
        return 0;
    }

    if (verbose) {
        Logger::setLogLevel(LogLevel::Debug);
        Logger::setDeveloperMode(true);
    }

    // Build command line string
    std::ostringstream oss;
    for (size_t i = 0; i < commandTokens.size(); ++i) {
        oss << commandTokens[i];
        if (i + 1 < commandTokens.size()) oss << " ";
    }
    std::string commandLine = oss.str();

    IpcClient daemon(socketPath);
    bool daemonRunning = daemon.isDaemonRunning();

    if (direct && daemonRunning) {
        std::cerr << "Error: sonyd is running and may own the Bluetooth session. Stop sonyd before using --direct.\n";
        return 1;
    }

    if (!direct) {
        if (!daemonRunning) {
            std::cerr << "Error: sonyd is not running at " << socketPath << "\n"
                      << "Start it with `sonyd`, or pass --direct for a one-off direct Bluetooth session.\n";
            return 1;
        }
        // sonyd connects to the headphones on demand rather than holding the
        // link permanently (see IpcServer), so the first command after an
        // idle period pays for a fresh Bluetooth connection here.
        auto resp = daemon.sendCommand(commandLine, std::chrono::seconds(15));
        if (resp.success) {
            if (!resp.data.empty()) {
                std::cout << resp.data << "\n";
            } else if (!resp.message.empty()) {
                std::cout << resp.message << "\n";
            }
            return 0;
        } else {
            std::cerr << "Error: " << resp.message << "\n";
            return 1;
        }
    }

    // Direct mode: bypass the daemon and open a one-off Bluetooth session.
    std::cerr << "Using a direct Bluetooth session (--direct).\n";

    std::shared_ptr<ITransport> transport = transport::createPlatformTransport();
    std::shared_ptr<IDeviceDiscovery> discovery = transport::createPlatformDiscovery();
    DeviceService service(transport, discovery);

    auto cmd = IpcProtocol::parseCommand(commandLine);
    if (cmd.type != IpcCommandType::Devices) {
        auto devs = service.discoverDevices();
        if (!devs.empty()) {
            try {
                service.connect(DeviceAddress(devs.front().address), devs.front().name);
            } catch (const std::exception& ex) {
                std::cerr << "Notice: initial connection to " << devs.front().name << " deferred: " << ex.what() << "\n";
            }
        }
    }

    auto resp = IpcProtocol::execute(cmd, service);


    if (resp.success) {
        if (!resp.data.empty()) {
            std::cout << resp.data << "\n";
        } else if (!resp.message.empty()) {
            std::cout << resp.message << "\n";
        }
        return 0;
    } else {
        std::cerr << "Error: " << resp.message << "\n";
        return 1;
    }
}
