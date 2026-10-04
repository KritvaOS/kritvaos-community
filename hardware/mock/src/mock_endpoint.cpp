//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : mock_endpoint.cpp
// Description : Mock fault-injection schedule and settings.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Mock Hardware
// Layer       : Hardware Abstraction
//
// Requirements: DER-701; DER-702
// API         : kritva::hardware::mock::Injection
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <kritva/hardware/mock/mock_endpoint.hpp>

#include <variant>

namespace kritva::hardware::mock {

namespace {

core::Result<std::int64_t> read_count(const core::Configuration& cfg, const std::string& name, std::int64_t min, std::int64_t max) {
    const core::Parameter* p = cfg.get(name);
    if (p == nullptr) return core::Result<std::int64_t>::success(min == 0 ? 0 : min);
    const auto* v = std::get_if<std::int64_t>(&p->value);
    if (v == nullptr || *v < min || *v > max) {
        return core::Result<std::int64_t>::failure(core::Error{core::ErrorCode::CONFIGURATION_ERROR, core::ErrorSeverity::ERROR, {}, {},
            "'" + name + "' must be an integer in [" + std::to_string(min) + ", " + std::to_string(max) + "]"});
    }
    return core::Result<std::int64_t>::success(*v);
}

} // namespace

core::Result<void> Injection::configure(const core::Configuration& scoped, const std::string& unit) {
    const auto fail = read_count(scoped, "fail_after_" + unit, 0, 1'000'000);
    if (!fail) return core::Result<void>::failure(fail.error());
    const auto fault = read_count(scoped, "fault_after_" + unit, 0, 1'000'000);
    if (!fault) return core::Result<void>::failure(fault.error());
    fail_after = fail.value();
    fault_after = fault.value();
    return core::Result<void>::success();
}

bool Injection::take_failure() noexcept {
    if (fail_next) {
        fail_next = false;
        return true;
    }
    if (fail_after > 0 && !failed_once && ok_operations == static_cast<std::uint64_t>(fail_after)) {
        failed_once = true;
        return true;
    }
    return false;
}

} // namespace kritva::hardware::mock
