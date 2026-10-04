//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : diagnostics.hpp
// Description : Device and endpoint diagnostics: detached snapshots and a deterministic text rendering.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Diagnostics
// Layer       : Hardware Abstraction
//
// Requirements: DER-601..608; DER-701; DER-702; DER-703
// API         : kritva::hardware::DeviceDiagnostics / EndpointDiagnostics / describe
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <kritva/hardware/identity.hpp>

namespace kritva::hardware {

/// Detached snapshot of one Endpoint, read from the live object when taken (nothing is
/// mirrored, so it cannot disagree with the endpoint, DER-602, DER-604, DER-606).
struct EndpointDiagnostics {
    DeviceId device_id;
    std::string device_name;
    EndpointId endpoint_id;
    std::string endpoint_name;
    EndpointDirection direction{EndpointDirection::SENSOR};
    bool enabled{true};                         ///< false: the device is switched off and the endpoint is inert
    core::LifecycleState lifecycle{core::LifecycleState::UNKNOWN};
    core::StatusCode status{core::StatusCode::UNKNOWN};
    core::HealthState health{core::HealthState::UNKNOWN};
    /// The reason while UNHEALTHY or DEGRADED (for a FAULT endpoint: the deterministic
    /// failure reason, DER-701); empty otherwise.
    std::string health_detail;
    std::uint64_t operations_ok{0};             ///< successful data operations
    std::uint64_t operations_failed{0};         ///< failed data operations (refused ones included)
    std::optional<core::Error> last_error;      ///< the most recent failure of any operation
    std::vector<std::string> capabilities;      ///< capability names (DER-605)
};

/// Snapshot of one Device and its endpoints in enumeration order.
struct DeviceDiagnostics {
    DeviceId id;
    std::string name;
    bool enabled{true};
    core::StatusCode status{core::StatusCode::UNKNOWN};     ///< aggregated by the Device
    core::HealthState health{core::HealthState::UNKNOWN};   ///< aggregated by the Device
    std::string health_detail;
    std::vector<std::string> capabilities;
    std::vector<EndpointDiagnostics> endpoints;
};

/// Deterministic multi-line text for a snapshot: one line per device and per endpoint, the
/// same input always giving the same text. It contains identities, states, counters, error
/// codes and error messages only; control characters in messages are replaced by '?' so one
/// failure cannot forge extra lines, and no configuration value is ever included (DER-608).
[[nodiscard]] std::string describe(const std::vector<DeviceDiagnostics>& devices);

} // namespace kritva::hardware
