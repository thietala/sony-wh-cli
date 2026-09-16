#pragma once

#include "SemanticTypes.h"
#include <array>
#include <string>

namespace sony::protocol {

enum class ProtocolGeneration {
    V1,
    V2
};

class IProtocol {
public:
    virtual ~IProtocol() = default;

    [[nodiscard]] virtual ProtocolGeneration generation() const noexcept = 0;

    virtual void initDevice() = 0;

    virtual BatteryState getBattery() = 0;

    virtual NoiseControlState getNoiseControl() = 0;
    virtual void setNoiseControl(const NoiseControlState& state) = 0;

    virtual EqualizerState getEqualizer() = 0;
    virtual void setEqualizerPreset(int preset) = 0;
    virtual void setEqualizerCustom(int clearBass, const std::array<int, 5>& bands) = 0;

    virtual bool getDsee() = 0;
    virtual void setDsee(bool enabled) = 0;

    virtual std::string getFirmwareVersion() = 0;
    virtual std::string getCodec() = 0;

    virtual int getAutoPowerOff() = 0;
    virtual void setAutoPowerOff(int index) = 0;

    virtual bool getSpeakToChat() = 0;
    virtual void setSpeakToChat(bool enabled) = 0;

    virtual bool getAdaptiveVolume() = 0;
    virtual void setAdaptiveVolume(bool enabled) = 0;

    // Initializes headphone settings (Sony's own wording for this action);
    // the device disconnects shortly after. Confirmed working by packet
    // capture and real-hardware testing on a WH-1000XM5 — only implemented
    // where confirmed (see DeviceCapabilities::reset).
    virtual void reset() = 0;

    // Factory reset: wipes the pairing itself, not just settings. Confirmed
    // by real-hardware testing to require a full re-pair afterward — only
    // implemented where confirmed (see DeviceCapabilities::factoryReset).
    virtual void factoryReset() = 0;
};

} // namespace sony::protocol
