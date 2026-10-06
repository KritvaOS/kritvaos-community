//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : capability_dispatch.hpp
// Description : The only place that maps a wire capability onto a concrete typed I3 endpoint contract.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: ER-002; ER-003; SR-001
// API         : kritva::hardware::remote::classify / read_sensor / write_motor_command
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <cstdint>
#include <optional>

#include <kritva/hardware/endpoint.hpp>
#include <kritva/hardware/transport/codec.hpp>

namespace kritva::hardware::remote {

/// The endpoint kinds protocol 1.0 can carry (spec section 13), by capability id:
/// 0x1001 acceleration, 0x1002 angular velocity, 0x1003 position, 0x2001 motor command.
enum class EndpointKind : std::uint8_t { ACCELERATION, ANGULAR_VELOCITY, POSITION, MOTOR_COMMAND };

[[nodiscard]] constexpr std::uint64_t capability_id_of(EndpointKind kind) noexcept {
    switch (kind) {
        case EndpointKind::ACCELERATION: return 0x1001;
        case EndpointKind::ANGULAR_VELOCITY: return 0x1002;
        case EndpointKind::POSITION: return 0x1003;
        case EndpointKind::MOTOR_COMMAND: return 0x2001;
    }
    return 0;
}

/// The kind of `endpoint`, or nothing if protocol 1.0 cannot carry it. An endpoint is supported iff it has
/// exactly ONE capability, that capability is one of the four ids above, the endpoint's direction matches
/// (sensor for the first three, actuator for the last) and the object really is the typed I3 endpoint of
/// that kind (SensorEndpoint<AccelerationSample> and so on). Nothing here changes any I3 contract.
[[nodiscard]] std::optional<EndpointKind> classify(const Endpoint& endpoint);

/// One sensor sample in wire form (a Vec3 sample or a scalar one).
struct SensorReading {
    transport::ReadKind kind{transport::ReadKind::VEC3};
    double x{0}, y{0}, z{0};
    double value{0};
    std::uint64_t sample_sequence{0};
    std::int64_t timestamp_ns{0};
};

/// Reads one sample through the endpoint's own SensorEndpoint<T>::read(). The I3 state check, counters and
/// error behavior are the endpoint's: the error of a failing read is returned unchanged. INVALID_ARGUMENT
/// if the endpoint is not a supported sensor.
[[nodiscard]] core::Result<SensorReading> read_sensor(Endpoint& endpoint);

/// Writes one command through the endpoint's own ActuatorEndpoint<MotorCommand>::write(): the I3 state
/// check and the endpoint's limit validation run unchanged, before anything is applied. The error of a
/// failing write is returned unchanged. INVALID_ARGUMENT if the endpoint is not a motor command endpoint.
[[nodiscard]] core::Result<void> write_motor_command(Endpoint& endpoint, double velocity_rad_s);

} // namespace kritva::hardware::remote
