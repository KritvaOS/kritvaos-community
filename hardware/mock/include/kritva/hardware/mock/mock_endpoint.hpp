//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : mock_endpoint.hpp
// Description : Shared mock behavior: deterministic fault injection and degradation for mock endpoints.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Mock Hardware
// Layer       : Hardware Abstraction
//
// Requirements: DER-701; DER-702; DER-703; DER-802
// API         : kritva::hardware::mock::MockEndpoint / Injection
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <kritva/hardware/device.hpp>
#include <kritva/hardware/typed_endpoints.hpp>

namespace kritva::hardware::mock {

/// The failure schedule of one mock endpoint, counted in SUCCESSFUL operations of the
/// current live period (reset by initialize()). Everything is deterministic: the same
/// settings and the same call sequence always give the same outcome.
///
/// Settings (int, 0 = never; 0..1000000), named `fail_after_<unit>` and
/// `fault_after_<unit>` where <unit> is `ops` (sensors) or `writes` (actuators):
///  - fail_after_*  : after N successful operations the NEXT operation fails once with
///                    INTERNAL_ERROR; the endpoint stays RUNNING and later operations succeed.
///  - fault_after_* : after the Nth successful operation the endpoint enters FAULT
///                    (that operation still succeeds); every later operation fails with
///                    RESOURCE_UNAVAILABLE and nothing restarts it.
struct Injection {
    std::int64_t fail_after{0};
    std::int64_t fault_after{0};
    bool fail_next{false};
    bool failed_once{false};
    std::uint64_t ok_operations{0};

    /// Reads the two settings from `scoped` (bare names built from `unit`); anything
    /// out of range or not an integer is a CONFIGURATION_ERROR.
    core::Result<void> configure(const core::Configuration& scoped, const std::string& unit);
    void new_live_period() noexcept { ok_operations = 0; failed_once = false; fail_next = false; }
    /// True iff this operation must fail now (and consumes the scheduled failure).
    [[nodiscard]] bool take_failure() noexcept;
    [[nodiscard]] bool fault_due() const noexcept { return fault_after > 0 && ok_operations == static_cast<std::uint64_t>(fault_after); }
};

/// Adds the test-facing controls shared by all mock endpoints on top of a typed
/// endpoint interface. `Unit` is the word used in the setting names ("ops"/"writes").
template <class Base>
class MockEndpoint : public Base {
public:

    /// Puts a live endpoint into FAULT as a hardware failure would (idempotent; INVALID_STATE
    /// if the endpoint is not live). Used by tests and the demo to inject a fault.
    core::Result<void> inject_fault(const char* reason = "injected fault") {
        return this->enter_fault(this->make_error(core::ErrorCode::INTERNAL_ERROR, reason));
    }
    /// The next operation fails once with INTERNAL_ERROR.
    void fail_next_operation() noexcept { injection_.fail_next = true; }
    /// Reports degradation (health DEGRADED while RUNNING) until cleared.
    void degrade(std::string detail) { this->set_degraded(std::move(detail)); }
    void clear_degradation() { this->clear_degraded(); }

protected:
    explicit MockEndpoint(EndpointInfo info, core::CapabilitySet caps, std::string unit)
        : Base(std::move(info), std::move(caps)), unit_(std::move(unit)) {}

    [[nodiscard]] std::vector<std::string> injection_settings() const {
        return {"fail_after_" + unit_, "fault_after_" + unit_};
    }
    core::Result<void> on_initialize() override {
        injection_.new_live_period();
        return core::Result<void>::success();
    }
    core::Result<void> configure_injection(const core::Configuration& scoped) { return injection_.configure(scoped, unit_); }

    /// Call at the start of an operation: the scheduled failure, if due.
    core::Result<void> scheduled_failure(const char* what) {
        if (!injection_.take_failure()) return core::Result<void>::success();
        return core::Result<void>::failure(this->make_error(core::ErrorCode::INTERNAL_ERROR, std::string("injected ") + what + " failure"));
    }
    /// Call after a successful operation: counts it and applies a scheduled fault.
    void operation_succeeded() {
        ++injection_.ok_operations;
        if (injection_.fault_due()) (void)this->enter_fault(this->make_error(core::ErrorCode::INTERNAL_ERROR, "injected fault after the scheduled operations"));
    }

    Injection injection_;

private:
    std::string unit_;
};

} // namespace kritva::hardware::mock
