//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : codec_samples.hpp
// Description : Representative valid payloads of every message type and a type-dispatching decode/re-encode helper (test only).
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: FR-002; FR-003; PR-002
// API         : all_samples / decode_and_reencode
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <kritva/hardware/transport/codec.hpp>

namespace kritva::hardware::transport::test {

struct Sample {
    MessageType type;
    ReadKind kind;                          // meaningful for READ_RESPONSE only
    std::string name;
    std::vector<std::uint8_t> payload;      // a valid, canonically encoded payload
};

inline std::string repeat(char c, std::size_t n) { return std::string(n, c); }

inline AddressPayload addr() { return AddressPayload{1, 2, 3}; }

template <class T>
std::vector<std::uint8_t> must(const core::Result<std::vector<std::uint8_t>>& r, const T&) {
    if (!r) { std::fprintf(stderr, "sample did not encode: %s\n", r.error().message.c_str()); std::exit(2); }
    return r.value();
}

inline StatusField err(core::ErrorCode c, std::string m) { return StatusField{c, std::move(m)}; }

/// A discovery of `devices` devices with `endpoints_per_device` endpoints each, all names at the maximum length.
inline DiscoveryResponsePayload big_discovery(std::size_t devices, std::size_t endpoints_per_device, std::size_t name_length = 64) {
    DiscoveryResponsePayload d;
    for (std::size_t i = 0; i < devices; ++i) {
        DiscoveredDevice dev;
        dev.id = i + 1;
        dev.name = std::string(name_length - 3, 'd') + std::to_string(100 + i);      // unique, name_length long
        for (std::size_t k = 0; k < endpoints_per_device; ++k) {
            DiscoveredEndpoint e;
            e.id = k + 1;
            e.name = std::string(name_length - 3, 'e') + std::to_string(100 + k);
            e.direction = k % 2 ? WireDirection::ACTUATOR : WireDirection::SENSOR;
            e.capabilities.push_back({0x1001, std::string(name_length, 'c')});
            dev.endpoints.push_back(std::move(e));
        }
        d.devices.push_back(std::move(dev));
    }
    return d;
}

inline std::vector<Sample> all_samples() {
    std::vector<Sample> s;
    const auto add = [&](MessageType t, const char* name, std::vector<std::uint8_t> bytes, ReadKind kind = ReadKind::VEC3) {
        s.push_back(Sample{t, kind, name, std::move(bytes)});
    };
    add(MessageType::HELLO, "hello", must(encode(HelloPayload{7, 1, 0, 100, 300}), 0));
    add(MessageType::HELLO, "hello_max", must(encode(HelloPayload{~0ull, 65535, 65535, 0xFFFFFFFF, 0xFFFFFFFF}), 0));
    add(MessageType::HELLO_ACK, "hello_ack", must(encode(HelloAckPayload{{}, 9, 1, 0, 42, 100, 300}), 0));
    add(MessageType::HELLO_ACK, "hello_ack_error", must(encode(HelloAckPayload{err(core::ErrorCode::UNSUPPORTED, "unsupported protocol version"), 0, 1, 0, 0, 0, 0}), 0));
    add(MessageType::DISCOVERY_REQUEST, "discovery_request", {});
    {
        DiscoveryResponsePayload d;
        DiscoveredDevice imu{10, "left_arm_imu", {}};
        imu.endpoints.push_back({1, "acceleration", WireDirection::SENSOR, {{0x1001, "acceleration"}}});
        imu.endpoints.push_back({2, "angular_velocity", WireDirection::SENSOR, {{0x1002, "angular_velocity"}}});
        DiscoveredDevice motor{20, "shoulder_motor", {}};
        motor.endpoints.push_back({1, "command", WireDirection::ACTUATOR, {{0x2001, "motor_command"}}});
        motor.endpoints.push_back({2, "position", WireDirection::SENSOR, {{0x1003, "position"}}});
        d.devices = {imu, motor};
        add(MessageType::DISCOVERY_RESPONSE, "discovery", must(encode(d), 0));
    }
    add(MessageType::DISCOVERY_RESPONSE, "discovery_empty", must(encode(DiscoveryResponsePayload{}), 0));
    add(MessageType::DISCOVERY_RESPONSE, "discovery_device_without_endpoints", must(encode(big_discovery(1, 0)), 0));
    add(MessageType::DISCOVERY_RESPONSE, "discovery_error", must(encode(DiscoveryResponsePayload{err(core::ErrorCode::UNSUPPORTED, "topology too large"), {}}), 0));
    add(MessageType::DISCOVERY_RESPONSE, "discovery_max", must(encode(big_discovery(64, 4)), 0));
    {
        ConfigureRequestPayload c{addr(), {{"limit", std::int64_t{-5}}, {"enabled", true}, {"note", std::string("a b c")}, {"flag", false}}};
        add(MessageType::CONFIGURE_REQUEST, "configure", must(encode(c), 0));
        ConfigureRequestPayload many{addr(), {}};
        for (int i = 0; i < 16; ++i) many.settings.push_back({"k" + std::to_string(i), std::string(256, 'x')});
        add(MessageType::CONFIGURE_REQUEST, "configure_max", must(encode(many), 0));
        add(MessageType::CONFIGURE_REQUEST, "configure_none", must(encode(ConfigureRequestPayload{addr(), {}}), 0));
    }
    for (auto t : {MessageType::INITIALIZE_REQUEST, MessageType::START_REQUEST, MessageType::STOP_REQUEST, MessageType::SHUTDOWN_REQUEST,
                   MessageType::READ_REQUEST, MessageType::OBSERVE_REQUEST}) add(t, message_type_name(t), must(encode(addr()), 0));
    for (auto t : {MessageType::CONFIGURE_RESPONSE, MessageType::INITIALIZE_RESPONSE, MessageType::START_RESPONSE, MessageType::STOP_RESPONSE,
                   MessageType::SHUTDOWN_RESPONSE}) {
        add(t, message_type_name(t), must(encode(LifecycleResponsePayload{{}, core::LifecycleState::RUNNING}), 0));
    }
    add(MessageType::START_RESPONSE, "start_response_error", must(encode(LifecycleResponsePayload{err(core::ErrorCode::INVALID_STATE, "start is not valid in this lifecycle state"), {}}), 0));
    add(MessageType::READ_RESPONSE, "read_vec3", must(encode(ReadResponsePayload{{}, ReadKind::VEC3, 1.5, -2.5, 9.81, 0, 12, 12'000'000}), 0), ReadKind::VEC3);
    add(MessageType::READ_RESPONSE, "read_scalar", must(encode(ReadResponsePayload{{}, ReadKind::SCALAR, 0, 0, 0, -0.25, 3, 3'000'000}), 0), ReadKind::SCALAR);
    add(MessageType::READ_RESPONSE, "read_error", must(encode(ReadResponsePayload{err(core::ErrorCode::NOT_READY, "read failed: the endpoint is not running"), ReadKind::VEC3}), 0), ReadKind::VEC3);
    add(MessageType::WRITE_REQUEST, "write_request", must(encode(WriteRequestPayload{addr(), 0.5}), 0));
    add(MessageType::WRITE_RESPONSE, "write_response", must(encode(StatusPayload{}), 0));
    add(MessageType::WRITE_RESPONSE, "write_response_error", must(encode(StatusPayload{err(core::ErrorCode::INVALID_ARGUMENT, "motor command is outside the allowed range")}), 0));
    {
        ObserveResponsePayload o;
        o.state = core::LifecycleState::FAULT; o.status_code = core::StatusCode::FAILED; o.health = core::HealthState::UNHEALTHY;
        o.health_detail = "injected demo fault"; o.operations_ok = 3; o.operations_failed = 1; o.has_last_error = true;
        o.last_error_code = core::ErrorCode::INTERNAL_ERROR; o.last_error_message = "injected demo fault";
        add(MessageType::OBSERVE_RESPONSE, "observe_fault", must(encode(o), 0));
        ObserveResponsePayload h;
        h.state = core::LifecycleState::RUNNING; h.status_code = core::StatusCode::OK; h.health = core::HealthState::HEALTHY;
        add(MessageType::OBSERVE_RESPONSE, "observe_healthy", must(encode(h), 0));
    }
    add(MessageType::FAULT_EVENT, "fault_event", must(encode(FaultEventPayload{addr(), core::LifecycleState::FAULT, "link lost"}), 0));
    add(MessageType::HEARTBEAT, "heartbeat", must(encode(HeartbeatPayload{123456789}), 0));
    add(MessageType::PROTOCOL_ERROR, "protocol_error", must(encode_protocol_error(StatusPayload{err(core::ErrorCode::INVALID_ARGUMENT, "bad payload")}), 0));
    return s;
}

/// Decodes `payload` as the payload of `type` (using `kind` for READ_RESPONSE) and encodes the decoded value again.
/// Returns the decode outcome; `reencoded` is filled only on success.
inline FrameError decode_and_reencode(MessageType type, ReadKind kind, ByteSpan payload, std::vector<std::uint8_t>& reencoded) {
    const auto go = [&](auto decoded) -> FrameError {
        if (!decoded.ok()) return decoded.error;
        const auto e = encode(decoded.value);
        if (!e) return FrameError::UNKNOWN_TYPE;               // a decoded value must always re-encode: signal a contract break
        reencoded = e.value();
        return FrameError::NONE;
    };
    switch (type) {
        case MessageType::HELLO: return go(decode<HelloPayload>(payload));
        case MessageType::HELLO_ACK: return go(decode<HelloAckPayload>(payload));
        case MessageType::DISCOVERY_REQUEST: reencoded.clear(); return decode_empty(payload);
        case MessageType::DISCOVERY_RESPONSE: return go(decode<DiscoveryResponsePayload>(payload));
        case MessageType::CONFIGURE_REQUEST: return go(decode<ConfigureRequestPayload>(payload));
        case MessageType::CONFIGURE_RESPONSE: case MessageType::INITIALIZE_RESPONSE: case MessageType::START_RESPONSE:
        case MessageType::STOP_RESPONSE: case MessageType::SHUTDOWN_RESPONSE: return go(decode<LifecycleResponsePayload>(payload));
        case MessageType::INITIALIZE_REQUEST: case MessageType::START_REQUEST: case MessageType::STOP_REQUEST:
        case MessageType::SHUTDOWN_REQUEST: case MessageType::READ_REQUEST: case MessageType::OBSERVE_REQUEST:
            return go(decode<AddressPayload>(payload));
        case MessageType::READ_RESPONSE: return go(decode_read_response(payload, kind));
        case MessageType::WRITE_REQUEST: return go(decode<WriteRequestPayload>(payload));
        case MessageType::WRITE_RESPONSE: return go(decode<StatusPayload>(payload));
        case MessageType::OBSERVE_RESPONSE: return go(decode<ObserveResponsePayload>(payload));
        case MessageType::FAULT_EVENT: return go(decode<FaultEventPayload>(payload));
        case MessageType::HEARTBEAT: return go(decode<HeartbeatPayload>(payload));
        case MessageType::PROTOCOL_ERROR: {
            const auto d = decode_protocol_error(payload);
            if (!d.ok()) return d.error;
            const auto e = encode_protocol_error(d.value);
            if (!e) return FrameError::UNKNOWN_TYPE;
            reencoded = e.value();
            return FrameError::NONE;
        }
    }
    return FrameError::UNKNOWN_TYPE;
}

} // namespace kritva::hardware::transport::test
