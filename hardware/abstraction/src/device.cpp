//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device.cpp
// Description : Device endpoint ownership and aggregation.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Device
// Layer       : Hardware Abstraction
//
// Requirements: DER-105; DER-106; DER-206; DER-601; DER-603; DER-605
// API         : kritva::hardware::Device
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <kritva/hardware/device.hpp>

namespace kritva::hardware {

using core::ErrorCode;
using core::HealthState;
using core::LifecycleState;
using core::Result;

core::Result<Endpoint*> Device::add_endpoint(std::unique_ptr<Endpoint> endpoint) {
    const auto fail = [](ErrorCode code, const char* message) {
        return Result<Endpoint*>::failure(core::Error{code, core::ErrorSeverity::ERROR, {}, {}, message});
    };
    if (!endpoint) return fail(ErrorCode::INVALID_ARGUMENT, "endpoint must not be null");
    if (sealed_) return fail(ErrorCode::INVALID_STATE, "the device is sealed");
    for (const Endpoint* existing : view_) {
        if (existing->lifecycle_state() != LifecycleState::UNKNOWN) {
            return fail(ErrorCode::INVALID_STATE, "the endpoint set is fixed once an endpoint has been used");
        }
    }
    if (endpoint->lifecycle_state() != LifecycleState::UNKNOWN) {
        return fail(ErrorCode::INVALID_STATE, "a used endpoint cannot be added");
    }
    if (find_endpoint(endpoint->info().id()) != nullptr) return fail(ErrorCode::INVALID_ARGUMENT, "duplicate endpoint id in this device");
    if (find_endpoint(endpoint->info().name()) != nullptr) return fail(ErrorCode::INVALID_ARGUMENT, "duplicate endpoint name in this device");

    Endpoint* raw = endpoint.get();
    owned_.push_back(std::move(endpoint));
    view_.push_back(raw);
    return Result<Endpoint*>::success(raw);
}

Endpoint* Device::find_endpoint(EndpointId id) const noexcept {
    for (Endpoint* e : view_) if (e->info().id() == id) return e;
    return nullptr;
}

Endpoint* Device::find_endpoint(std::string_view name) const noexcept {
    for (Endpoint* e : view_) if (e->info().name() == name) return e;
    return nullptr;
}

core::Status Device::status() const {
    if (view_.empty()) return core::Status{core::StatusCode::UNKNOWN};
    bool all_ok = true;
    for (const Endpoint* e : view_) {
        const auto code = e->status().code();
        if (code == core::StatusCode::FAILED) return core::Status{core::StatusCode::FAILED};
        if (code != core::StatusCode::OK) all_ok = false;
    }
    return core::Status{all_ok ? core::StatusCode::OK : core::StatusCode::UNKNOWN};
}

core::Health Device::health() const {
    if (view_.empty()) return core::Health{HealthState::UNKNOWN};
    const Endpoint* degraded = nullptr;
    bool all_healthy = true;
    for (const Endpoint* e : view_) {
        const auto h = e->health();
        if (h.state() == HealthState::UNHEALTHY) {
            core::Health out{HealthState::UNHEALTHY};
            out.set_detail(e->info().name() + ": " + h.detail());
            return out;
        }
        if (h.state() == HealthState::DEGRADED && degraded == nullptr) degraded = e;
        if (h.state() != HealthState::HEALTHY) all_healthy = false;
    }
    if (degraded != nullptr) {
        core::Health out{HealthState::DEGRADED};
        out.set_detail(degraded->info().name() + ": " + degraded->health().detail());
        return out;
    }
    return core::Health{all_healthy ? HealthState::HEALTHY : HealthState::UNKNOWN};
}

core::CapabilitySet Device::capabilities() const {
    core::CapabilitySet all;
    for (const Endpoint* e : view_) for (const auto& c : e->capabilities().all()) all.add(c);
    return all;
}

} // namespace kritva::hardware
