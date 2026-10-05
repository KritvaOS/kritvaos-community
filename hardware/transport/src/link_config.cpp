//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : link_config.cpp
// Description : Link timing configuration loading.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Transport
// Layer       : Hardware Abstraction
//
// Requirements: TR-003
// API         : kritva::hardware::transport::link_config_keys / link_timing_from
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <kritva/hardware/transport/link_config.hpp>

#include <cstdint>
#include <variant>

namespace kritva::hardware::transport {

namespace {

using core::ErrorCode;

core::Error config_error(std::string message) {
    return core::Error{ErrorCode::CONFIGURATION_ERROR, core::ErrorSeverity::ERROR, {}, {}, std::move(message)};
}

// Reads one integer key into `out` when present; false (with a message) if present but not an integer in range.
bool read_u32(const core::Configuration& cfg, const char* key, std::uint32_t& out, std::string& message) {
    const core::Parameter* p = cfg.get(key);
    if (p == nullptr) return true;
    const auto* v = std::get_if<std::int64_t>(&p->value);
    if (v == nullptr || *v < 0 || *v > static_cast<std::int64_t>(UINT32_MAX)) {
        message = std::string("'") + key + "' must be a non-negative integer number of milliseconds";
        return false;
    }
    out = static_cast<std::uint32_t>(*v);
    return true;
}

} // namespace

std::vector<std::string> link_config_keys() {
    return {kKeyHeartbeatPeriodMs, kKeyHeartbeatTimeoutMs, kKeyRequestTimeoutMs, kKeyPumpQuantumMs};
}

core::Result<LinkTiming> link_timing_from(const core::Configuration& configuration) {
    LinkTiming t;
    std::string message;
    if (!read_u32(configuration, kKeyHeartbeatPeriodMs, t.heartbeat_period_ms, message) ||
        !read_u32(configuration, kKeyHeartbeatTimeoutMs, t.heartbeat_timeout_ms, message) ||
        !read_u32(configuration, kKeyRequestTimeoutMs, t.request_timeout_ms, message) ||
        !read_u32(configuration, kKeyPumpQuantumMs, t.pump_quantum_ms, message)) {
        return core::Result<LinkTiming>::failure(config_error(std::move(message)));
    }
    if (!is_valid(t)) {
        return core::Result<LinkTiming>::failure(config_error(
            "link timing out of range: heartbeat_period 10..60000, heartbeat_timeout 20..600000 and >= 2 x period, "
            "request_timeout 1..60000, pump_quantum 1..1000 and <= request_timeout (milliseconds)"));
    }
    return core::Result<LinkTiming>::success(t);
}

} // namespace kritva::hardware::transport
