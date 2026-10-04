//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : samples.hpp
// Description : Typed sensor samples and the motor command: units, validity, defaults, timestamp policy.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Typed Data
// Layer       : Hardware Abstraction
//
// Requirements: DER-501; DER-502; DER-505; DER-506; DER-507; DER-702
// API         : AccelerationSample / AngularVelocitySample / PositionSample / MotorCommand
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <cmath>
#include <cstdint>

#include <kritva/core/core.hpp>

namespace kritva::hardware {

// There is deliberately no common "any sample" type: each endpoint kind has its own
// plain value type (trivially copyable, default-constructible), so units and
// validity are stated once, per type (DER-507).
//
// UNITS are SI: metres, radians, seconds. Frames are the device's own; no frame
// transform is defined by I3.
//
// DEFAULT STATE. A default-constructed sample is all zero, `sequence` 0 and a zero
// timestamp. `sequence == 0` means "no sample has been produced into this object";
// a producing endpoint numbers its samples 1, 2, 3 ... with no gaps.
//
// VALIDITY. A sample is valid iff every numeric value is finite (is_valid()).
// SensorEndpoint::read() enforces it: it never reports success with an invalid sample,
// and the caller's sample object is left unchanged when read() fails (DER-503, DER-505).
//
// TIMESTAMP POLICY (DER-702). `timestamp` is MONOTONIC-domain time since the start of
// the endpoint's live period. I3 endpoints are simulated and use a deterministic
// virtual clock: sample n is stamped `n * period` (see virtual_timestamp()); no wall
// clock is read, so identical runs produce identical samples. A real driver will
// supply a hardware time through the same field.
//
// REAL TIME. None of this is real-time (DER-506).

/// Three components in the device frame.
struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

[[nodiscard]] inline bool is_finite(const Vec3& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

/// Linear acceleration in m/s^2 (gravity included, as an accelerometer measures it).
struct AccelerationSample {
    Vec3 value;
    std::uint64_t sequence{0};
    core::Timestamp timestamp{};
};

/// Angular velocity in rad/s about the device axes.
struct AngularVelocitySample {
    Vec3 value;
    std::uint64_t sequence{0};
    core::Timestamp timestamp{};
};

/// Joint position in rad.
struct PositionSample {
    double value{0.0};
    std::uint64_t sequence{0};
    core::Timestamp timestamp{};
};

/// Motor command: a target joint velocity in rad/s (positive and negative are the two
/// directions of the joint). A single mode; there is no mode selector.
///
/// SAFETY. A command can move a physical system. An implementation must validate it
/// (validate()) BEFORE acting and must treat a rejected command as having no effect.
struct MotorCommand {
    double velocity_rad_s{0.0};
};

/// Allowed command range in rad/s, inclusive at both ends. The default is the
/// conservative range [-1, 1]; an implementation overrides it from configuration.
struct MotorLimits {
    double min_rad_s{-1.0};
    double max_rad_s{1.0};
};

[[nodiscard]] inline bool is_valid(const AccelerationSample& s) noexcept { return is_finite(s.value); }
[[nodiscard]] inline bool is_valid(const AngularVelocitySample& s) noexcept { return is_finite(s.value); }
[[nodiscard]] inline bool is_valid(const PositionSample& s) noexcept { return std::isfinite(s.value); }
[[nodiscard]] inline bool is_valid(const MotorCommand& c) noexcept { return std::isfinite(c.velocity_rad_s); }

/// Limits are usable iff both are finite and min <= max.
[[nodiscard]] inline core::Result<void> validate_limits(const MotorLimits& limits) {
    if (!std::isfinite(limits.min_rad_s) || !std::isfinite(limits.max_rad_s) || limits.min_rad_s > limits.max_rad_s) {
        return core::Result<void>::failure(core::Error{core::ErrorCode::INVALID_ARGUMENT, core::ErrorSeverity::ERROR, {}, {},
                                                       "motor limits must be finite with min <= max"});
    }
    return core::Result<void>::success();
}

/// Accepts a command iff it is finite and within `limits` (inclusive); otherwise
/// INVALID_ARGUMENT, and the caller must not act on the command.
[[nodiscard]] inline core::Result<void> validate(const MotorCommand& command, const MotorLimits& limits) {
    if (!std::isfinite(command.velocity_rad_s)) {
        return core::Result<void>::failure(core::Error{core::ErrorCode::INVALID_ARGUMENT, core::ErrorSeverity::ERROR, {}, {},
                                                       "motor command must be a finite number"});
    }
    if (command.velocity_rad_s < limits.min_rad_s || command.velocity_rad_s > limits.max_rad_s) {
        return core::Result<void>::failure(core::Error{core::ErrorCode::INVALID_ARGUMENT, core::ErrorSeverity::ERROR, {}, {},
                                                       "motor command is outside the allowed range"});
    }
    return core::Result<void>::success();
}

/// Deterministic virtual time of sample number `sequence` (1-based): sequence * period.
[[nodiscard]] constexpr core::Timestamp virtual_timestamp(std::uint64_t sequence,
                                                          std::int64_t period_ns = 1'000'000) noexcept {
    return core::Timestamp(static_cast<std::int64_t>(sequence) * period_ns, core::ClockDomain::MONOTONIC);
}

} // namespace kritva::hardware
