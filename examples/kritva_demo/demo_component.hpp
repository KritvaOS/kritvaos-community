//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : demo_component.hpp
// Description : Shared lifecycle bookkeeping for the demo components (Core Component contract).
//
// Component   : KritvaOS Demo
// Module      : Reference Demo
// Layer       : Application
//
// Requirements: RR-CMP-001..008; RR-FLT-001..003
// API         : kritva::demo::DemoComponent
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <string>
#include <utility>

#include <kritva/core/core.hpp>

namespace kritva::demo {

/// Implements the Core runtime::Component lifecycle table once for the demo
/// components: configure/initialize valid from UNKNOWN and STOPPED, start from
/// READY, stop from READY and RUNNING, shutdown from UNKNOWN/STOPPED (no-op) and
/// FAULT (-> STOPPED); every other call fails with INVALID_STATE. A failed
/// component stays in FAULT / UNHEALTHY until shutdown(); there is no recovery.
/// Every returned Error carries this component's id as its source.
class DemoComponent : public core::runtime::Component {
public:
    core::Result<void> configure(const core::Configuration& configuration) override {
        if (!in(core::LifecycleState::UNKNOWN) && !in(core::LifecycleState::STOPPED)) return invalid("configure");
        return with_source(on_configure(configuration));
    }
    core::Result<void> initialize() override {
        if (!in(core::LifecycleState::UNKNOWN) && !in(core::LifecycleState::STOPPED)) return invalid("initialize");
        state_ = core::LifecycleState::READY;
        return core::Result<void>::success();
    }
    core::Result<void> start() override {
        if (!in(core::LifecycleState::READY)) return invalid("start");
        state_ = core::LifecycleState::RUNNING;
        return core::Result<void>::success();
    }
    core::Result<void> stop() override {
        if (!in(core::LifecycleState::READY) && !in(core::LifecycleState::RUNNING)) return invalid("stop");
        state_ = core::LifecycleState::STOPPED;
        return core::Result<void>::success();
    }
    core::Result<void> shutdown() override {
        if (in(core::LifecycleState::FAULT)) { state_ = core::LifecycleState::STOPPED; detail_.clear(); }
        else if (!in(core::LifecycleState::UNKNOWN) && !in(core::LifecycleState::STOPPED)) return invalid("shutdown");
        return core::Result<void>::success();
    }

    core::LifecycleState lifecycle_state() const noexcept override { return state_; }
    core::Status status() const override {
        using core::StatusCode;
        return core::Status{state_ == core::LifecycleState::FAULT ? StatusCode::FAILED
                            : state_ == core::LifecycleState::UNKNOWN ? StatusCode::UNKNOWN : StatusCode::OK};
    }
    core::Health health() const override {
        core::Health h;
        if (state_ == core::LifecycleState::RUNNING) h.set_state(core::HealthState::HEALTHY);
        if (state_ == core::LifecycleState::FAULT) { h.set_state(core::HealthState::UNHEALTHY); h.set_detail(detail_); }
        return h;
    }
    core::CapabilitySet capabilities() const override { return core::CapabilitySet{}; }

protected:
    DemoComponent(std::uint64_t id, std::string name)
        : Component(core::runtime::ComponentInfo::create(core::runtime::ComponentId{id}, std::move(name)).value()) {}

    /// Component-specific configuration; the default accepts anything.
    virtual core::Result<void> on_configure(const core::Configuration&) { return core::Result<void>::success(); }

    [[nodiscard]] bool in(core::LifecycleState s) const noexcept { return state_ == s; }

    /// Component-local failure: FAULT / UNHEALTHY with a reason. Idempotent.
    void enter_fault(std::string reason) {
        state_ = core::LifecycleState::FAULT;
        detail_ = std::move(reason);
    }

    template <class T>
    core::Result<T> with_source(core::Result<T> r) const {
        if (r.has_value()) return r;
        core::Error e = r.error();
        e.source = info().id();
        return core::Result<T>::failure(std::move(e));
    }

private:
    core::Result<void> invalid(const char* operation) const {
        return core::Result<void>::failure(core::Error{
            core::ErrorCode::INVALID_STATE, core::ErrorSeverity::ERROR, info().id(), {},
            std::string(operation) + " is not valid in this lifecycle state"});
    }

    core::LifecycleState state_{core::LifecycleState::UNKNOWN};
    std::string detail_;
};

} // namespace kritva::demo
