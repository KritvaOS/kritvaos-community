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
    notify_fault();
    return failed;
}

// Finishes initialize/start/stop after its hook ran. A hook that already faulted the endpoint
// (enter_fault) must not cause a second transition or notification.
Result<void> Endpoint::complete(Result<void> hook_result, LifecycleState on_success) {
    if (lifecycle_.state() == LifecycleState::FAULT) {
        if (!hook_result) return hook_result;
        return Result<void>::failure(last_error_ ? *last_error_ : make_error(ErrorCode::INTERNAL_ERROR, "the endpoint faulted during the operation"));
    }
    if (!hook_result) return fail_into_fault(std::move(hook_result));
    transition(on_success);
    return Result<void>::success();
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
    return complete(on_initialize(), LifecycleState::READY);
}

Result<void> Endpoint::start() {
    if (lifecycle_.state() != LifecycleState::READY) return invalid_state("start");
    return complete(on_start(), LifecycleState::RUNNING);
}

Result<void> Endpoint::stop() {
    const auto s = lifecycle_.state();
    if (s != LifecycleState::READY && s != LifecycleState::RUNNING) return invalid_state("stop");
    if (s == LifecycleState::RUNNING) transition(LifecycleState::STOPPING);
    auto r = complete(on_stop(), LifecycleState::STOPPED);
    if (r) degraded_detail_.reset();
    return r;
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
    notify_fault();
    return Result<void>::success();
}

void Endpoint::notify_fault() {
    on_fault();                                           // make the hardware safe first
    for (const auto& entry : fault_listeners_) {
        if (entry.second) entry.second(*this);
    }
}

void Endpoint::set_fault_listener(const void* owner, FaultListener listener) {
    for (auto& entry : fault_listeners_) {
        if (entry.first == owner) { entry.second = std::move(listener); return; }
    }
    fault_listeners_.emplace_back(owner, std::move(listener));
}

void Endpoint::clear_fault_listener(const void* owner) noexcept {
    for (auto it = fault_listeners_.begin(); it != fault_listeners_.end(); ++it) {
        if (it->first == owner) { fault_listeners_.erase(it); return; }
    }
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
