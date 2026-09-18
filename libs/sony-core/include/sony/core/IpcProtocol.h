#pragma once

#include "IDeviceService.h"
#include <string>
#include <string_view>
#include <vector>

namespace sony::core {

enum class IpcCommandType {
    Devices,
    Info,
    Battery,
    Anc,
    Ambient,
    EqGet,
    EqPreset,
    EqCustom,
    Dsee,
    AutoPowerOff,
    SpeakToChat,
    AdaptiveVolume,
    Status,
    Reset,
    FactoryReset,
    Raw,
    Unknown
};

struct IpcCommand {
    IpcCommandType type{IpcCommandType::Unknown};
    std::vector<std::string> args;
    std::string raw;
};

struct IpcResponse {
    bool success{true};
    std::string message;
    std::string data;
};

// Argument a client must include to run a destructive command. The daemon has
// no terminal to prompt on, so it requires this token in the request itself;
// sony-wh-cli asks the human first and only then adds it.
inline constexpr std::string_view kConfirmArg = "--yes";

class IpcProtocol {
public:
    static std::string executeLine(std::string_view line, IDeviceService& service);
    static IpcCommand parseCommand(std::string_view line);
    static std::string serializeResponse(const IpcResponse& response);
    static IpcResponse parseResponse(std::string_view line);
    static IpcResponse execute(const IpcCommand& cmd, IDeviceService& service);

    // True for a destructive command that arrived without kConfirmArg. Such a
    // command is refused before it touches the device (or triggers a connect).
    [[nodiscard]] static bool needsConfirmation(const IpcCommand& cmd);
};

} // namespace sony::core
