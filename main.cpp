#include "sony/core/DeviceService.h"
#include "sony/core/IpcClient.h"
#include "sony/core/IpcProtocol.h"
#include "sony/protocol/EqualizerPresets.h"
#include "sony/protocol/FrameCodec.h"
#include "sony/transport/FakeTransport.h"
#include "sony/transport/PlatformTransport.h"
#include "sony/transport/Logger.h"


#include <algorithm>
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
              << "Usage: sony-wh-cli [options] <command> [args...]\n"
              << "       sony-wh-cli help <command>          Show detailed help for a command\n\n"
              << "Query commands:\n"
              << "  devices                    List discovered paired Sony devices\n"
              << "  info                       Display connected device information & capabilities\n"
              << "  battery                    Display battery percentage and charging state\n"
              << "  status                     Display connection status\n\n"
              << "Control commands:\n"
              << "  anc on|off                 Enable or disable Active Noise Cancelling\n"
              << "  ambient <1-20>|off         Set Ambient Sound level or turn ambient off\n"
              << "  eq get|preset|custom       Get or set the Equalizer (see: help eq)\n"
              << "  dsee on|off                Toggle DSEE sound enhancement\n"
              << "  apo <0-5>                  Set Auto-Power-Off duration preset index\n"
              << "  speaktochat on|off         Toggle Speak-to-Chat auto-pause\n"
              << "  adaptivevolume on|off      Toggle Adaptive Volume\n\n"
              << "Destructive commands:\n"
              << "  reset                      Initialize headphone settings (disconnects the\n"
              << "                             device; confirmed on WH-1000XM5 only)\n"
              << "  factoryreset               DESTRUCTIVE: wipes the pairing itself; headphones\n"
              << "                             must be re-paired afterward (WH-1000XM5 only)\n\n"
              << "Options:\n"
              << "  -s, --socket <path>        Custom Unix domain socket path for sonyd\n"
              << "  --direct                   Bypass sonyd for a one-off direct Bluetooth\n"
              << "                             session (refuses to run alongside a live sonyd)\n"
              << "  -v, --verbose              Enable verbose diagnostic logging\n"
              << "  -h, --help                 Display this help menu; after a command, shows\n"
              << "                             detailed help for that command instead\n\n"
              << "sony-wh-cli requires sonyd to be running; start it with `sonyd` first,\n"
              << "or pass --direct to skip the daemon for a single command.\n\n"
              << "Examples:\n"
              << "  sony-wh-cli anc on\n"
              << "  sony-wh-cli ambient 10\n"
              << "  sony-wh-cli eq bass-boost\n"
              << "  sony-wh-cli dsee on\n"
              << "  sony-wh-cli help eq\n";
}

void printEqualizerPresetList() {
    std::cout << "Available presets:\n";
    for (const auto& info : equalizerPresets()) {
        std::cout << "  " << info.id << "\n";
    }
}

// Detailed, per-command help shown by `sony-wh-cli help <command>` or
// `sony-wh-cli <command> --help`. Unlike the one-line summaries in
// printHelp(), this can show things like the live equalizer preset list
// pulled straight from EqualizerPresets.h instead of a hand-copied excerpt.
void printCommandHelp(const std::string& command) {
    std::string cmd = command;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) { return std::tolower(c); });

    if (cmd == "devices") {
        std::cout << "devices — list discovered paired Sony devices\n\n"
                  << "Usage: sony-wh-cli devices\n\n"
                  << "Lists every nearby/paired Sony device with its Bluetooth address.\n"
                  << "Does not require an existing connection.\n";
    } else if (cmd == "info") {
        std::cout << "info — display connected device information & capabilities\n\n"
                  << "Usage: sony-wh-cli info\n\n"
                  << "Shows the model name, protocol version, firmware, codec, battery,\n"
                  << "noise control state, and which features this device supports.\n";
    } else if (cmd == "battery") {
        std::cout << "battery — display battery percentage and charging state\n\n"
                  << "Usage: sony-wh-cli battery\n\n"
                  << "For earbuds with separate left/right/case batteries, shows each one\n"
                  << "individually when the device reports them.\n";
    } else if (cmd == "status") {
        std::cout << "status — display connection status\n\n"
                  << "Usage: sony-wh-cli status\n";
    } else if (cmd == "anc") {
        std::cout << "anc — enable or disable Active Noise Cancelling\n\n"
                  << "Usage: sony-wh-cli anc <on|off>\n";
    } else if (cmd == "ambient") {
        std::cout << "ambient — set Ambient Sound level, or turn it off\n\n"
                  << "Usage:\n"
                  << "  sony-wh-cli ambient <1-20>   Enable Ambient Sound at this level\n"
                  << "  sony-wh-cli ambient off      Turn Ambient Sound off\n";
    } else if (cmd == "eq") {
        std::cout << "eq — get or set the Equalizer\n\n"
                  << "Usage:\n"
                  << "  sony-wh-cli eq get                     Show the active preset and band levels\n"
                  << "  sony-wh-cli eq preset <name>           Set a preset by name\n"
                  << "  sony-wh-cli eq <name>                  Shorthand for 'eq preset <name>'\n"
                  << "  sony-wh-cli eq custom <cb> <b1..b5>    Apply custom Clear Bass and 5 bands\n"
                  << "                                         (each -10..10)\n\n";
        printEqualizerPresetList();
    } else if (cmd == "dsee") {
        std::cout << "dsee — toggle DSEE sound enhancement\n\n"
                  << "Usage: sony-wh-cli dsee <on|off>\n";
    } else if (cmd == "apo" || cmd == "autopoweroff") {
        std::cout << "apo — set the Auto-Power-Off duration\n\n"
                  << "Usage: sony-wh-cli apo <0-5>\n\n"
                  << "Index meanings:\n"
                  << "  0  Off\n"
                  << "  1  5 minutes\n"
                  << "  2  30 minutes\n"
                  << "  3  1 hour\n"
                  << "  4  3 hours\n"
                  << "  5  When taken off\n";
    } else if (cmd == "speaktochat") {
        std::cout << "speaktochat — toggle Speak-to-Chat auto-pause\n\n"
                  << "Usage: sony-wh-cli speaktochat <on|off>\n\n"
                  << "Automatically pauses playback when you start speaking.\n";
    } else if (cmd == "adaptivevolume") {
        std::cout << "adaptivevolume — toggle Adaptive Volume\n\n"
                  << "Usage: sony-wh-cli adaptivevolume <on|off>\n\n"
                  << "Automatically adjusts volume based on ambient noise and activity.\n";
    } else if (cmd == "reset") {
        std::cout << "reset — initialize headphone settings\n\n"
                  << "Usage: sony-wh-cli reset\n\n"
                  << "Resets settings (EQ, ANC, etc.) to their defaults; the headphones\n"
                  << "disconnect briefly and sonyd reconnects automatically afterward.\n"
                  << "Confirmed on WH-1000XM5 only — refuses on other models rather than\n"
                  << "guessing.\n";
    } else if (cmd == "factoryreset") {
        std::cout << "factoryreset — DESTRUCTIVE: factory reset\n\n"
                  << "Usage: sony-wh-cli factoryreset\n\n"
                  << "Wipes the pairing itself, not just settings. The headphones will\n"
                  << "need to be re-paired from your device's Bluetooth settings\n"
                  << "afterward. Confirmed on WH-1000XM5 only — refuses on other models\n"
                  << "rather than guessing.\n";
    } else {
        std::cout << "No detailed help for '" << command << "'.\n"
                  << "Run `sony-wh-cli --help` for the list of commands.\n";
    }
}

} // namespace

int main(int argc, char* argv[]) {
    std::string socketPath = defaultSocketPath();
    bool direct = false;
    bool verbose = false;
    bool helpRequested = false;
    std::vector<std::string> commandTokens;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            helpRequested = true;
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

    // `sony-wh-cli help <command>` and `sony-wh-cli <command> --help` both
    // show detailed help for that one command instead of the full list.
    if (!commandTokens.empty() && commandTokens[0] == "help") {
        if (commandTokens.size() > 1) printCommandHelp(commandTokens[1]);
        else printHelp();
        return 0;
    }
    if (helpRequested) {
        if (!commandTokens.empty()) printCommandHelp(commandTokens[0]);
        else printHelp();
        return 0;
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
