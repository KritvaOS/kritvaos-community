//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : edge_rig.hpp
// Description : Test rig: an EdgeHost over mock devices on a simulated link, driven by hand-built Nexus frames.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: ER-001; ER-002; ER-003
// API         : EdgeRig
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "../../runtime/check.hpp"

#include <kritva/hardware/mock/mock_imu.hpp>
#include <kritva/hardware/mock/mock_motor.hpp>
#include <kritva/hardware/remote/edge_host.hpp>
#include <kritva/hardware/transport/simulated_transport.hpp>

namespace kritva::hardware::remote::test {

using namespace kritva::hardware::transport;
using Bytes = std::vector<std::uint8_t>;

struct Reply {
    FrameHeader header;
    Bytes payload;
};

inline constexpr std::uint64_t kEdgeNode = 7;
inline constexpr std::uint64_t kNexusNode = 3;
inline constexpr std::uint64_t kImu = 1;            // device ids
inline constexpr std::uint64_t kMotor = 2;
inline constexpr std::uint64_t kAccel = 1;          // imu endpoint ids
inline constexpr std::uint64_t kGyro = 2;
inline constexpr std::uint64_t kCommand = 1;        // motor endpoint ids
inline constexpr std::uint64_t kPosition = 2;

inline Bytes must(const core::Result<Bytes>& r) {
    KRITVA_CHECK(r.has_value());
    return r.value();
}

inline Bytes addr(std::uint64_t device, std::uint64_t endpoint, std::uint64_t node = kEdgeNode) {
    return must(encode(AddressPayload{node, device, endpoint}));
}

class EdgeRig {
public:
    SimulatedTransport link;
    mock::MockImuDevice imu{DeviceInfo::create(DeviceId{kImu}, "imu").value()};
    mock::MockMotorDevice motor{DeviceInfo::create(DeviceId{kMotor}, "motor").value()};
    DeviceRegistry registry;
    std::unique_ptr<EdgeHost> edge;
    std::uint64_t session{0};                       // the Nexus's idea of the current session
    std::uint64_t seq{0};                           // the Nexus's frame counter in that session

    explicit EdgeRig(bool with_devices = true) {
        KRITVA_CHECK(link.connect().has_value());
        if (with_devices) {
            KRITVA_CHECK(registry.register_device(imu).has_value());
            KRITVA_CHECK(registry.register_device(motor).has_value());
        }
        auto created = EdgeHost::create(NodeId{kEdgeNode}, registry, link.edge());
        KRITVA_CHECK(created.has_value());
        edge = std::move(created.value());
    }

    [[nodiscard]] const EdgeStats& stats() const { return edge->stats(); }

    // ---- frames -----------------------------------------------------------------------------------------
    static Bytes frame(MessageType type, const Bytes& payload, std::uint64_t sequence, std::uint64_t session_id,
                       std::uint64_t correlation = 0) {
        FrameHeader h;
        h.type = type;
        h.sequence = sequence;
        h.correlation_id = correlation;
        h.session_id = session_id;
        return must(encode_frame(h, payload));
    }

    void send_raw(const Bytes& bytes) { KRITVA_CHECK(link.nexus().send(bytes).has_value()); }

    // Everything the Edge has answered since the last call, after it handled what was receivable.
    std::vector<Reply> poll() {
        edge->poll();
        return drain();
    }

    std::vector<Reply> drain() {
        std::vector<Reply> out;
        while (auto f = link.nexus().receive()) {
            const DecodedFrame d = decode_frame(*f, Receiver::NEXUS);
            KRITVA_CHECK(d.ok());
            out.push_back({d.header, Bytes(d.payload.begin(), d.payload.end())});
        }
        return out;
    }

    // A request with the next sequence of the current session. Returns the sequence used.
    std::uint64_t send(MessageType type, const Bytes& payload) {
        const std::uint64_t s = ++seq;
        send_raw(frame(type, payload, s, session));
        return s;
    }

    // Notices the Edge sent (HEARTBEAT, FAULT_EVENT) that call() set aside, in arrival order.
    std::vector<Reply> notices;

    // Send and expect exactly one reply, correlated to the request. Notices that arrive with it are kept in `notices`.
    Reply call(MessageType type, const Bytes& payload) {
        const std::uint64_t s = send(type, payload);
        auto all = poll();
        std::vector<Reply> replies;
        for (auto& r : all) {
            if (r.header.type == MessageType::HEARTBEAT || r.header.type == MessageType::FAULT_EVENT) notices.push_back(std::move(r));
            else replies.push_back(std::move(r));
        }
        KRITVA_CHECK(replies.size() == 1);
        KRITVA_CHECK(replies[0].header.correlation_id == s);
        KRITVA_CHECK(replies[0].header.type == *response_for(type));
        return replies[0];
    }

    // HELLO: a new session from the Nexus's point of view. Returns the HELLO_ACK payload.
    HelloAckPayload hello(std::uint16_t major = 1, std::uint16_t minor = 0, std::uint32_t period = 100, std::uint32_t timeout = 300) {
        seq = 1;
        send_raw(frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, major, minor, period, timeout})), 1, 0));
        auto replies = poll();
        KRITVA_CHECK(replies.size() == 1);
        const auto ack = decode<HelloAckPayload>(replies[0].payload);
        KRITVA_CHECK(ack.ok());
        if (ack.value.status.ok()) session = ack.value.session_id;
        return ack.value;
    }

    // ---- typed calls -------------------------------------------------------------------------------------
    LifecycleResponsePayload lifecycle(MessageType request, std::uint64_t device, std::uint64_t endpoint) {
        const Reply r = call(request, addr(device, endpoint));
        const auto d = decode<LifecycleResponsePayload>(r.payload);
        KRITVA_CHECK(d.ok());
        return d.value;
    }
    StatusPayload write(std::uint64_t device, std::uint64_t endpoint, double velocity) {
        const Reply r = call(MessageType::WRITE_REQUEST,
                             must(encode(WriteRequestPayload{AddressPayload{kEdgeNode, device, endpoint}, velocity})));
        const auto d = decode<StatusPayload>(r.payload);
        KRITVA_CHECK(d.ok());
        return d.value;
    }
    ReadResponsePayload read(std::uint64_t device, std::uint64_t endpoint, ReadKind kind) {
        const Reply r = call(MessageType::READ_REQUEST, addr(device, endpoint));
        const auto d = decode_read_response(r.payload, kind);
        KRITVA_CHECK(d.ok());
        return d.value;
    }
    ObserveResponsePayload observe(std::uint64_t device, std::uint64_t endpoint) {
        const Reply r = call(MessageType::OBSERVE_REQUEST, addr(device, endpoint));
        const auto d = decode<ObserveResponsePayload>(r.payload);
        KRITVA_CHECK(d.ok());
        return d.value;
    }
    DiscoveryResponsePayload discover() {
        const Reply r = call(MessageType::DISCOVERY_REQUEST, {});
        const auto d = decode<DiscoveryResponsePayload>(r.payload);
        KRITVA_CHECK(d.ok());
        return d.value;
    }

    // Initialize and start every endpoint of both devices (the order is the Nexus's business).
    void bring_up_all() {
        for (const auto& [d, e] : all_endpoints()) {
            KRITVA_CHECK(lifecycle(MessageType::INITIALIZE_REQUEST, d, e).status.ok());
            KRITVA_CHECK(lifecycle(MessageType::START_REQUEST, d, e).status.ok());
        }
    }
    static std::vector<std::pair<std::uint64_t, std::uint64_t>> all_endpoints() {
        return {{kImu, kAccel}, {kImu, kGyro}, {kMotor, kCommand}, {kMotor, kPosition}};
    }
};

} // namespace kritva::hardware::remote::test
