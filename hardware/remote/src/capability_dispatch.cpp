//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : capability_dispatch.cpp
// Description : Capability-to-typed-endpoint dispatch.
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

#include <kritva/hardware/remote/capability_dispatch.hpp>

#include <kritva/hardware/typed_endpoints.hpp>

namespace kritva::hardware::remote {

namespace {

core::Error make_error(core::ErrorCode code, const char* message) {
    return core::Error{code, core::ErrorSeverity::ERROR, {}, {}, message};
}

SensorReading reading(const Vec3& v, std::uint64_t sequence, core::Timestamp t) {
    SensorReading r;
    r.kind = transport::ReadKind::VEC3;
    r.x = v.x;
    r.y = v.y;
    r.z = v.z;
    r.sample_sequence = sequence;
    r.timestamp_ns = t.nanoseconds();
    return r;
}

} // namespace

std::optional<EndpointKind> classify(const Endpoint& endpoint) {
    const auto& all = endpoint.capabilities().all();
    if (all.size() != 1) return std::nullopt;
    const std::uint64_t id = all.front().id.value();
    const EndpointDirection direction = endpoint.info().direction();
    if (direction == EndpointDirection::SENSOR) {
        if (id == capability_id_of(EndpointKind::ACCELERATION) && dynamic_cast<const AccelerationEndpoint*>(&endpoint) != nullptr) return EndpointKind::ACCELERATION;
        if (id == capability_id_of(EndpointKind::ANGULAR_VELOCITY) && dynamic_cast<const AngularVelocityEndpoint*>(&endpoint) != nullptr) return EndpointKind::ANGULAR_VELOCITY;
        if (id == capability_id_of(EndpointKind::POSITION) && dynamic_cast<const PositionEndpoint*>(&endpoint) != nullptr) return EndpointKind::POSITION;
        return std::nullopt;
    }
    if (id == capability_id_of(EndpointKind::MOTOR_COMMAND) && dynamic_cast<const MotorCommandEndpoint*>(&endpoint) != nullptr) return EndpointKind::MOTOR_COMMAND;
    return std::nullopt;
}

core::Result<SensorReading> read_sensor(Endpoint& endpoint) {
    const auto kind = classify(endpoint);
    if (!kind || *kind == EndpointKind::MOTOR_COMMAND) {
        return core::Result<SensorReading>::failure(make_error(core::ErrorCode::INVALID_ARGUMENT, "the endpoint is not a supported sensor"));
    }
    switch (*kind) {
        case EndpointKind::ACCELERATION: {
            AccelerationSample s;
            auto r = static_cast<AccelerationEndpoint&>(endpoint).read(s);
            if (!r) return core::Result<SensorReading>::failure(r.error());
            return core::Result<SensorReading>::success(reading(s.value, s.sequence, s.timestamp));
        }
        case EndpointKind::ANGULAR_VELOCITY: {
            AngularVelocitySample s;
            auto r = static_cast<AngularVelocityEndpoint&>(endpoint).read(s);
            if (!r) return core::Result<SensorReading>::failure(r.error());
            return core::Result<SensorReading>::success(reading(s.value, s.sequence, s.timestamp));
        }
        case EndpointKind::POSITION: {
            PositionSample s;
            auto r = static_cast<PositionEndpoint&>(endpoint).read(s);
            if (!r) return core::Result<SensorReading>::failure(r.error());
            SensorReading out;
            out.kind = transport::ReadKind::SCALAR;
            out.value = s.value;
            out.sample_sequence = s.sequence;
            out.timestamp_ns = s.timestamp.nanoseconds();
            return core::Result<SensorReading>::success(out);
        }
        case EndpointKind::MOTOR_COMMAND: break;
    }
    return core::Result<SensorReading>::failure(make_error(core::ErrorCode::INVALID_ARGUMENT, "the endpoint is not a supported sensor"));
}

core::Result<void> write_motor_command(Endpoint& endpoint, double velocity_rad_s) {
    const auto kind = classify(endpoint);
    if (!kind || *kind != EndpointKind::MOTOR_COMMAND) {
        return core::Result<void>::failure(make_error(core::ErrorCode::INVALID_ARGUMENT, "the endpoint is not a motor command endpoint"));
    }
    return static_cast<MotorCommandEndpoint&>(endpoint).write(MotorCommand{velocity_rad_s});
}

} // namespace kritva::hardware::remote
