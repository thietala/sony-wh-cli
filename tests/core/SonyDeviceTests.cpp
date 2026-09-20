#include <catch2/catch_test_macros.hpp>
#include "sony/core/SonyDevice.h"
#include "sony/protocol/FrameCodec.h"
#include "sony/transport/FakeTransport.h"
#include "sony/transport/SonyError.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

using namespace sony;
using namespace sony::core;
using namespace sony::protocol;
using namespace sony::transport;

namespace {

class AutoAckFakeTransport : public FakeTransport {
public:
    size_t send(std::span<const std::byte> data) override {
        size_t res = FakeTransport::send(data);
        if (!data.empty()) {
            std::vector<uint8_t> bytes(data.size());
            std::transform(data.begin(), data.end(), bytes.begin(), [](std::byte b) {
                return static_cast<uint8_t>(b);
            });
            try {
                auto frame = FrameCodec::decode(bytes);
                if (frame.type == DataType::DataMdr) {
                    SonyFrame ackFrame{
                        .type = DataType::Ack, .sequence = frame.sequence, .payload = {}};
                    queueIncoming(FrameCodec::encode(ackFrame));

                    // Auto-respond to known inquiry requests
                    if (!frame.payload.empty()) {
                        uint8_t op = frame.payload[0];
                        uint8_t subtype = frame.payload.size() > 1 ? frame.payload[1] : 0x00;
                        if (op == 0x00) { // Init query
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = _nextRespSeq(),
                                .payload = {0x01, 0x00}
                            }));
                        } else if (op == 0x22) { // Battery query
                            // Echo the requested subtype (main/dual/case) so
                            // every sub-query matches immediately instead of
                            // burning its 1s sendAndAwaitResponse timeout.
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = _nextRespSeq(),
                                .payload = {0x23, subtype, 85, 0x00}
                            }));
                        } else if (op == 0x66) { // NC query
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = _nextRespSeq(),
                                .payload = {0x67, 0x17, 0x01, 0x01, 0x00, 0x00, 0x00}
                            }));
                        } else if (op == 0x56) { // EQ query
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = _nextRespSeq(),
                                .payload = {
                                            0x57, 0x00, 0x00, 0x06, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a}
                            }));
                        } else if (op == 0xe6) { // DSEE query
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = _nextRespSeq(),
                                .payload = {0xe7, 0x01, 0x00}
                            }));
                        }
                    }
                }
            } catch (...) {}
        }
        return res;
    }

private:
    uint8_t _nextRespSeq() { return _respSeq++; }
    uint8_t _respSeq{0};
};

} // namespace

TEST_CASE("SonyDevice lifecycle and profile identification", "[core][device]") {
    auto transport = std::make_shared<AutoAckFakeTransport>();
    SonyDevice dev(transport, SonyProtocolVersion::V2);

    CHECK_FALSE(dev.isConnected());
    CHECK(dev.protocolVersion() == SonyProtocolVersion::V2);
    CHECK(dev.profile().model == SonyModel::Unknown);

    DeviceAddress addr("11:22:33:44:55:66");
    dev.connect(addr, "WH-1000XM5");

    CHECK(dev.isConnected());
    CHECK(dev.profile().model == SonyModel::WH1000XM5);
    CHECK(dev.capabilities().noiseCancelling);
    CHECK(dev.capabilities().ambientSound);
    CHECK(dev.capabilities().equalizer);
    CHECK(dev.capabilities().dsee);
    CHECK(dev.capabilities().speakToChat);
    CHECK(dev.capabilities().reset);
    CHECK(dev.capabilities().factoryReset);

    dev.disconnect();
    CHECK_FALSE(dev.isConnected());
}

TEST_CASE("SonyDevice gates reset to models with a confirmed opcode", "[core][device]") {
    auto transport = std::make_shared<AutoAckFakeTransport>();

    SonyDevice xm5(transport, SonyProtocolVersion::V2);
    xm5.connect(DeviceAddress("11:22:33:44:55:66"), "WH-1000XM5");
    REQUIRE_NOTHROW(xm5.reset());
    REQUIRE_NOTHROW(xm5.factoryReset());

    // Same V2 protocol, but reset hasn't been confirmed on this model, so
    // SonyDevice must refuse before ever touching the wire.
    auto otherTransport = std::make_shared<AutoAckFakeTransport>();
    SonyDevice xm5buds(otherTransport, SonyProtocolVersion::V2);
    xm5buds.connect(DeviceAddress("22:33:44:55:66:77"), "WF-1000XM5");
    CHECK_FALSE(xm5buds.capabilities().reset);
    CHECK_FALSE(xm5buds.capabilities().factoryReset);
    auto sentBeforeReset = otherTransport->sentCount();
    REQUIRE_THROWS_AS(xm5buds.reset(), SonyException);
    REQUIRE_THROWS_AS(xm5buds.factoryReset(), SonyException);
    CHECK(otherTransport->sentCount() == sentBeforeReset);
}

TEST_CASE("SonyDevice refuses speak-to-chat and adaptive volume on models without them",
    "[core][device]") {
    // True only if fn() throws the Unsupported error (not some other failure).
    auto unsupported = [](auto&& fn) {
        try {
            fn();
        } catch (const SonyException& ex) {
            return ex.code() == SonyErrorCode::Unsupported;
        }
        return false;
    };

    SECTION("each feature is checked against its own capability flag") {
        // WF-1000XM4 has Speak-to-Chat but not Adaptive Volume, so a check that
        // accepted "either" flag would let the second call through.
        auto transport = std::make_shared<AutoAckFakeTransport>();
        SonyDevice buds(transport, SonyProtocolVersion::V2);
        buds.connect(DeviceAddress("11:22:33:44:55:66"), "WF-1000XM4");
        REQUIRE(buds.capabilities().speakToChat);
        REQUIRE_FALSE(buds.capabilities().adaptiveVolume);

        auto sent = transport->sentCount();
        REQUIRE_NOTHROW(buds.setSpeakToChat(true));
        CHECK(transport->sentCount() > sent);
        CHECK(buds.snapshot()->speakToChat == true);

        sent = transport->sentCount();
        CHECK(unsupported([&] { buds.setAdaptiveVolume(true); }));
        CHECK(transport->sentCount() == sent);           // nothing reached the wire
        CHECK(buds.snapshot()->adaptiveVolume == false); // local state untouched
    }

    SECTION("a model with neither feature refuses both") {
        auto transport = std::make_shared<AutoAckFakeTransport>();
        SonyDevice headphones(transport, SonyProtocolVersion::V2);
        headphones.connect(DeviceAddress("22:33:44:55:66:77"), "WH-CH720N");
        REQUIRE_FALSE(headphones.capabilities().speakToChat);
        REQUIRE_FALSE(headphones.capabilities().adaptiveVolume);

        const auto sent = transport->sentCount();
        CHECK(unsupported([&] { headphones.setSpeakToChat(true); }));
        CHECK(unsupported([&] { headphones.setAdaptiveVolume(true); }));
        CHECK(transport->sentCount() == sent);
        CHECK(headphones.snapshot()->speakToChat == false);
        CHECK(headphones.snapshot()->adaptiveVolume == false);
    }
}

TEST_CASE("SonyDevice control methods and state updates", "[core][device]") {
    auto transport = std::make_shared<AutoAckFakeTransport>();
    SonyDevice dev(transport, SonyProtocolVersion::V2);
    dev.connect(DeviceAddress("11:22:33:44:55:66"), "WH-1000XM5");

    std::atomic<int> stateChangeCount{0};
    std::atomic<int> ncChangeCount{0};
    std::atomic<int> eqChangeCount{0};

    dev.events().onStateChanged([&](const DeviceStateChanged&) { stateChangeCount.fetch_add(1); });

    dev.events().onNoiseControlChanged(
        [&](const NoiseControlChanged&) { ncChangeCount.fetch_add(1); });

    dev.events().onEqualizerChanged([&](const EqualizerChanged&) { eqChangeCount.fetch_add(1); });

    SECTION("ANC control") {
        dev.setAnc(true);
        auto snap1 = dev.snapshot();
        CHECK(snap1->noiseControl.mode == NoiseControlMode::NoiseCancelling);
        CHECK(ncChangeCount.load() >= 1);

        dev.setAnc(false);
        auto snap2 = dev.snapshot();
        CHECK(snap2->noiseControl.mode == NoiseControlMode::Off);
    }

    SECTION("Ambient control") {
        dev.setAmbient(15, false);
        auto snap = dev.snapshot();
        CHECK(snap->noiseControl.mode == NoiseControlMode::Ambient);
        CHECK(snap->noiseControl.ambientLevel == 15);
        CHECK_FALSE(snap->noiseControl.focusOnVoice);
    }

    SECTION("Equalizer preset control") {
        dev.setEqualizerPreset(0x16); // Bass Boost
        auto snap = dev.snapshot();
        CHECK(snap->equalizer.preset == 0x16);
        CHECK(eqChangeCount.load() >= 1);

        for (int preset : {-1, 256, 272}) {
            dev.setEqualizerPreset(preset);
            auto unchanged = dev.snapshot();
            CHECK(unchanged->equalizer.preset == snap->equalizer.preset);
            CHECK(unchanged->equalizer.clearBass == snap->equalizer.clearBass);
            CHECK(unchanged->equalizer.bands == snap->equalizer.bands);
        }
    }

    SECTION("Equalizer custom control") {
        dev.setEqualizerCustom(4, {1, 2, 3, 4, 5});
        auto snap = dev.snapshot();
        CHECK(snap->equalizer.preset == 0xa0);
        CHECK(snap->equalizer.clearBass == 4);
        CHECK(snap->equalizer.bands[0] == 1);
        CHECK(snap->equalizer.bands[4] == 5);
    }

    SECTION("Feature toggles") {
        dev.setDsee(true);
        CHECK(dev.snapshot()->dsee == true);

        dev.setAutoPowerOff(3);
        CHECK(dev.snapshot()->autoPowerOff == 3);

        dev.setSpeakToChat(true);
        CHECK(dev.snapshot()->speakToChat == true);

        dev.setAdaptiveVolume(true);
        CHECK(dev.snapshot()->adaptiveVolume == true);
    }

    dev.disconnect();
}

TEST_CASE("SonyDevice unsolicited notification dispatch", "[core][device]") {
    auto transport = std::make_shared<AutoAckFakeTransport>();
    SonyDevice dev(transport, SonyProtocolVersion::V2);
    dev.connect(DeviceAddress("11:22:33:44:55:66"), "WH-1000XM5");

    std::atomic<int> batteryLevel{-1};
    dev.events().onBatteryChanged([&](const BatteryChanged& evt) {
        if (evt.battery.main.has_value()) {
            batteryLevel.store(*evt.battery.main);
        }
    });

    // Device notification for battery: 0x25, 0x00, level 88, not charging
    SonyFrame batNotif{
        .type = DataType::DataMdr, .sequence = 100, .payload = {0x25, 0x00, 88, 0x00}
    };
    transport->queueIncoming(FrameCodec::encode(batNotif));

    // Allow background session thread to process notification
    for (int i = 0; i < 50 && batteryLevel.load() != 88; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CHECK(batteryLevel.load() == 88);
    CHECK(dev.snapshot()->battery.main.value_or(-1) == 88);

    dev.disconnect();
}
