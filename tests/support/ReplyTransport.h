#pragma once
#include "sony/transport/FakeTransport.h"
#include "sony/protocol/FrameCodec.h"
#include <atomic>
// Alternating sequence numbers and subtype-specific replies exercise real
// request matching rather than bypassing the session machinery.
class ReplyTransport : public sony::transport::FakeTransport {
public:
    std::vector<std::string> attempts;
    std::string failAddress;
    void connect(const sony::transport::DeviceAddress& address) override {
        attempts.push_back(address.str());
        if (address.str() == failAddress)
            throw sony::SonyException(sony::SonyErrorCode::TransportFailure, "Device is off");
        seq = 0;
        clearIncoming();
        FakeTransport::connect(address);
    }
    size_t send(std::span<const std::byte> data) override {
        const auto count = FakeTransport::send(data);
        std::vector<uint8_t> bytes;
        for (auto b : data)
            bytes.push_back(static_cast<uint8_t>(b));
        auto frame = sony::protocol::FrameCodec::decode(bytes);
        using namespace sony::protocol;
        if (frame.type != DataType::DataMdr) return count;
        queueIncoming(FrameCodec::encode(
            {DataType::Ack, static_cast<uint8_t>(1 - (frame.sequence & 1)), {}}));
        const auto& p = frame.payload;
        if (p.empty()) return count;
        std::vector<uint8_t> reply;
        if (p[0] == 0) reply = {1, 0};
        // GET 10 <type> -> RET 11 <type> ... (V1 battery; distinct from 0x22,
        // which is POWER OFF on V1 devices). Unanswered, ProtocolV1::getBattery()
        // burns its full per-query timeout three times over on every connect
        // to a V1-named device - exactly the kind of real-time wait that is
        // merely slow on a fast CI runner and a timeout on a slow one.
        if (p[0] == 0x10) {
            if (p[1] == 0)
                reply = {0x11, 0, 80, 0};
            else if (p[1] == 1)
                reply = {0x11, 1, 80, 0, 80, 0};
            else if (p[1] == 2)
                reply = {0x11, 2, 80, 0};
        }
        if (p[0] == 0x22) {
            if (p[1] == 9)
                reply = {0x23, 9, 81, 0, 79, 0};
            else
                reply = {0x23, p[1], 85, 0};
        }
        if (p[0] == 0x66) reply = {0x67, p[1], 1, 1, 0, 0, 0};
        if (p[0] == 0x56) reply = {0x57, 0, 0, 6, 10, 11, 12, 13, 14, 15};
        if (p[0] == 0xe6) reply = {0xe7, 1, 0};
        if (!reply.empty()) notify(reply);
        return count;
    }
    void notify(std::vector<uint8_t> payload) {
        queueIncoming(sony::protocol::FrameCodec::encode({sony::protocol::DataType::DataMdr,
            static_cast<uint8_t>(seq++ & 1),
            std::move(payload)}));
    }

private:
    std::atomic<unsigned> seq{0};
};
