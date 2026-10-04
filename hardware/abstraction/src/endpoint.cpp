//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : endpoint.cpp
// Description : Endpoint lifecycle operations over Core Lifecycle.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Endpoint
// Layer       : Hardware Abstraction
//
// Requirements: DER-401..404; DER-407; DER-701; DER-703
// API         : kritva::hardware::Endpoint
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <kritva/hardware/endpoint.hpp>

#include <cassert>

namespace kritva::hardware {

using core::ErrorCode;
using core::LifecycleState;
using core::Result;

core::Error Endpoint::make_error(ErrorCode code, std::string message) {
    return core::Error{code, core::ErrorSeverity::ERROR, {}, {}, std::move(message)};
}

void Endpoint::transition(LifecycleState target) {
    const auto r = lifecycle_.transition_to(target);   // the Core transition table decides
    assert(r.has_value());
    (void)r;
}

Result<void> Endpoint::invalid_state(const char* operation) {
    auto error = make_error(ErrorCode::INVALID_STATE, std::string(operation) + " is not valid in this lifecycle state");
    last_error_ = error;
    return Result<void>::failure(std::move(error));
}

// A failing initialize/start/stop leaves the endpoint FAULT; the hook's Error is returned unchanged.
Result<void> Endpoint::fail_into_fault(Result<void> failed) {
    last_error_ = failed.error();
    fault_detail_ = failed.error().message;
    transition(LifecycleState::FAULT);
    return failed;
}

Result<void> Endpoint::configure(const core::Configuration& configuration) {
    const auto s = lifecycle_.state();
    if (s != LifecycleState::UNKNOWN && s != LifecycleState::STOPPED) return invalid_state("configure");
    auto r = on_configure(configuration);
    if (!r) last_error_ = r.error();
    return r;
}

Result<void> Endpoint::initialize() {
    const auto s = lifecycle_.state();
    if (s != LifecycleState::UNKNOWN && s != LifecycleState::STOPPED) return invalid_state("initialize");
    transition(LifecycleState::INITIALIZING);
    live_ = true;
    degraded_detail_.reset();
    fault_detail_.clear();
    if (auto r = on_initialize(); !r) return fail_into_fault(std::move(r));
    transition(LifecycleState::READY);
    return Result<void>::success();
}

Result<void> Endpoint::start() {
    if (lifecycle_.state() != LifecycleState::READY) return invalid_state("start");
    if (auto r = on_start(); !r) return fail_into_fault(std::move(r));
    transition(LifecycleState::RUNNING);
    return Result<void>::success();
}

Result<void> Endpoint::stop() {
    const auto s = lifecycle_.state();
    if (s != LifecycleState::READY && s != LifecycleState::RUNNING) return invalid_state("stop");
    if (s == LifecycleState::RUNNING) transition(LifecycleState::STOPPING);
    if (auto r = on_stop(); !r) return fail_into_fault(std::move(r));
    transition(LifecycleState::STOPPED);
    degraded_detail_.reset();
    return Result<void>::success();
}

Result<void> Endpoint::shutdown() {
    const auto s = lifecycle_.state();
    if (s != LifecycleState::UNKNOWN && s != LifecycleState::STOPPED && s != LifecycleState::FAULT) return invalid_state("shutdown");
    if (live_) {                                          // release the live period once
        if (auto r = on_shutdown(); !r) {
            last_error_ = r.error();
            return r;                                     // state unchanged; a retry resumes here
        }
        live_ = false;
    }
    if (s == LifecycleState::FAULT) {
        transition(LifecycleState::STOPPED);
        fault_detail_.clear();
    }
    return Result<void>::success();
}

Result<void> Endpoint::check_operational(const char* operation) const {
    switch (lifecycle_.state()) {
        case LifecycleState::RUNNING:
            return Result<void>::success();
        case LifecycleState::FAULT:
            return Result<void>::failure(make_error(ErrorCode::RESOURCE_UNAVAILABLE,
                                                    std::string(operation) + " failed: the endpoint is faulted"));
        default:
            return Result<void>::failure(make_error(ErrorCode::NOT_READY,
                                                    std::string(operation) + " failed: the endpoint is not running"));
    }
}

Result<void> Endpoint::enter_fault(const core::Error& cause) {
    const auto s = lifecycle_.state();
    if (s == LifecycleState::FAULT) return Result<void>::success();
    if (s != LifecycleState::READY && s != LifecycleState::RUNNING) {
        return Result<void>::failure(make_error(ErrorCode::INVALID_STATE, "enter_fault requires a live endpoint"));
    }
    last_error_ = cause;
    fault_detail_ = cause.message;
    transition(LifecycleState::FAULT);
    return Result<void>::success();
}

void Endpoint::set_degraded(std::string detail) { degraded_detail_ = std::move(detail); }

void Endpoint::count_operation(bool succeeded) noexcept {
    if (succeeded) statistics_.sample_count.increment();
    else statistics_.error_count.increment();
}

core::Status Endpoint::status() const {
    switch (lifecycle_.state()) {
        case LifecycleState::FAULT:   return core::Status{core::StatusCode::FAILED};
        case LifecycleState::UNKNOWN: return core::Status{core::StatusCode::UNKNOWN};
        default:                      return core::Status{core::StatusCode::OK};
    }
}

core::Health Endpoint::health() const {
    switch (lifecycle_.state()) {
        case LifecycleState::FAULT: {
            core::Health h{core::HealthState::UNHEALTHY};
            h.set_detail(fault_detail_);
            return h;
        }
        case LifecycleState::RUNNING: {
            if (!degraded_detail_) return core::Health{core::HealthState::HEALTHY};
            core::Health h{core::HealthState::DEGRADED};
            h.set_detail(*degraded_detail_);
            return h;
        }
        default:
            return core::Health{core::HealthState::UNKNOWN};
    }
}

} // namespace kritva::hardware
