//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : typed_endpoints.hpp
// Description : Named typed endpoint interfaces and their capability descriptions.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Typed Data
// Layer       : Hardware Abstraction
//
// Requirements: DER-303; DER-501; DER-502; DER-605
// API         : AccelerationEndpoint / AngularVelocityEndpoint / PositionEndpoint / MotorCommandEndpoint
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <kritva/hardware/actuator_endpoint.hpp>
#include <kritva/hardware/samples.hpp>
#include <kritva/hardware/sensor_endpoint.hpp>

namespace kritva::hardware {

using AccelerationEndpoint = SensorEndpoint<AccelerationSample>;
using AngularVelocityEndpoint = SensorEndpoint<AngularVelocitySample>;
using PositionEndpoint = SensorEndpoint<PositionSample>;
using MotorCommandEndpoint = ActuatorEndpoint<MotorCommand>;

/// Capability ids of the typed endpoints, as advertised in Endpoint::capabilities().
/// A capability names a kind of function, so devices of different make share them.
inline constexpr core::CapabilityId kCapabilityAcceleration{0x1001};
inline constexpr core::CapabilityId kCapabilityAngularVelocity{0x1002};
inline constexpr core::CapabilityId kCapabilityPosition{0x1003};
inline constexpr core::CapabilityId kCapabilityMotorCommand{0x2001};

[[nodiscard]] inline core::CapabilitySet capability_set(core::CapabilityId id, const char* name) {
    core::CapabilitySet set;
    set.add(core::Capability{id, name, {}});
    return set;
}

[[nodiscard]] inline core::CapabilitySet acceleration_capability() { return capability_set(kCapabilityAcceleration, "acceleration"); }
[[nodiscard]] inline core::CapabilitySet angular_velocity_capability() { return capability_set(kCapabilityAngularVelocity, "angular_velocity"); }
[[nodiscard]] inline core::CapabilitySet position_capability() { return capability_set(kCapabilityPosition, "position"); }
[[nodiscard]] inline core::CapabilitySet motor_command_capability() { return capability_set(kCapabilityMotorCommand, "motor_command"); }

} // namespace kritva::hardware
