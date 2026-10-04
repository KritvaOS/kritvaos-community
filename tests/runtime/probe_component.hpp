//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : probe_component.hpp
// Description : Test component that records the runtime state seen in each hook (test only).
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-LIF-002; RR-LIF-006
// API         : ProbeComponent
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <string>
#include <utility>
#include <vector>

#include <kritva/runtime/runtime_host.hpp>

namespace kritva::runtime::test {

/// Records "<name>.<hook>:<runtime state>" into a shared log, so tests can
/// observe INITIALIZING / STOPPING (transient in Core) and call ordering.
/// Can be told to fail one hook with a given error code.
class ProbeComponent final : public core::runtime::Component {
public:
    enum class Hook { NONE, INITIALIZE, START, STOP };

    ProbeComponent(std::uint64_t id, std::string name, const core::runtime::RuntimeManager& rt,
                   std::vector<std::string>& log, Hook fail_at = Hook::NONE)
        : Component(core::runtime::ComponentInfo::create(core::runtime::ComponentId{id}, std::move(name)).value()),
          rt_(rt), log_(log), fail_at_(fail_at) {}

    core::Result<void> configure(const core::Configuration&) override { return core::Result<void>::success(); }
    core::Result<void> initialize() override { return step("initialize", Hook::INITIALIZE, core::LifecycleState::READY); }
    core::Result<void> start() override { return step("start", Hook::START, core::LifecycleState::RUNNING); }
    core::Result<void> stop() override { return step("stop", Hook::STOP, core::LifecycleState::STOPPED); }
    core::Result<void> shutdown() override { record("shutdown"); return core::Result<void>::success(); }

    core::LifecycleState lifecycle_state() const noexcept override { return state_; }
    core::Status status() const override { return core::Status{}; }
    core::Health health() const override { return core::Health{}; }
    core::CapabilitySet capabilities() const override { return core::CapabilitySet{}; }

private:
    void record(const char* hook) {
        log_.push_back(info().name() + "." + hook + ":" + std::string(to_string(rt_.state())));
    }

    core::Result<void> step(const char* hook, Hook which, core::LifecycleState on_success) {
        record(hook);
        if (fail_at_ == which) {
            state_ = core::LifecycleState::FAULT;
            return core::Result<void>::failure(core::Error{
                core::ErrorCode::INTERNAL_ERROR, core::ErrorSeverity::ERROR, info().id(), {}, "probe failure"});
        }
        state_ = on_success;
        return core::Result<void>::success();
    }

    const core::runtime::RuntimeManager& rt_;
    std::vector<std::string>& log_;
    Hook fail_at_;
    core::LifecycleState state_{core::LifecycleState::UNKNOWN};
};

} // namespace kritva::runtime::test
