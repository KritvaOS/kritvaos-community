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

#include <cstdio>
#include <cstdlib>
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

    core::Result<void> configure(const core::Configuration&) override {
        if (log_configure_) record("configure");
        return core::Result<void>::success();
    }

    /// Include configure() calls in the log (off by default so lifecycle logs stay focused).
    void log_configure(bool on) { log_configure_ = on; }
    core::Result<void> initialize() override { return step("initialize", Hook::INITIALIZE, core::LifecycleState::READY); }
    core::Result<void> start() override { return step("start", Hook::START, core::LifecycleState::RUNNING); }
    core::Result<void> stop() override { return step("stop", Hook::STOP, core::LifecycleState::STOPPED); }
    core::Result<void> shutdown() override {
        record("shutdown");
        state_ = core::LifecycleState::STOPPED;                      // Component contract: shutdown() leaves FAULT
        return core::Result<void>::success();
    }

    /// Sink for events the component reports about itself (via Core's ComponentEventReporter).
    void set_event_sink(core::runtime::IEventSink* sink) { sink_ = sink; }

    /// Simulates a failure while RUNNING: the component enters FAULT / UNHEALTHY
    /// by itself and reports an ERROR event. The Runtime is not told directly.
    void inject_failure() {
        state_ = core::LifecycleState::FAULT;
        if (sink_ != nullptr) {
            core::Event event;
            event.type = core::EventType::ERROR;
            event.severity = core::ErrorSeverity::ERROR;
            (void)core::runtime::ComponentEventReporter(*this, *sink_).report(event);
        }
    }

    core::LifecycleState lifecycle_state() const noexcept override { return state_; }
    core::Status status() const override {
        return core::Status{state_ == core::LifecycleState::FAULT ? core::StatusCode::FAILED : core::StatusCode::OK};
    }
    core::Health health() const override {
        switch (state_) {
            case core::LifecycleState::RUNNING: return core::Health{core::HealthState::HEALTHY};
            case core::LifecycleState::FAULT: { core::Health h{core::HealthState::UNHEALTHY}; h.set_detail("probe failure"); return h; }
            default: return core::Health{core::HealthState::UNKNOWN};
        }
    }
    core::CapabilitySet capabilities() const override { return core::CapabilitySet{}; }

private:
    void record(const char* hook) {
        log_.push_back(info().name() + "." + hook + ":" + std::string(to_string(rt_.state())));
    }

    core::Result<void> step(const char* hook, Hook which, core::LifecycleState on_success) {
        record(hook);
        if (state_ == core::LifecycleState::FAULT) {                 // Component contract: FAULT is left only by shutdown()
            return core::Result<void>::failure(core::Error{
                core::ErrorCode::INVALID_STATE, core::ErrorSeverity::ERROR, info().id(), {}, "component is in FAULT"});
        }
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
    bool log_configure_{false};
    core::runtime::IEventSink* sink_{nullptr};
    core::LifecycleState state_{core::LifecycleState::UNKNOWN};
};

/// Fixed statistics provider for observation tests.
class FixedStatistics final : public core::runtime::IComponentStatistics {
public:
    FixedStatistics(std::uint64_t samples, std::uint64_t errors) : samples_(samples), errors_(errors) {}
    core::Statistics statistics() const override {
        core::Statistics s;
        s.sample_count.increment(samples_);
        s.error_count.increment(errors_);
        return s;
    }
private:
    std::uint64_t samples_, errors_;
};

/// Applies a minimal valid configuration so the host may be initialized.
inline void configure_host(RuntimeHost& host) {
    const auto cfg = parse_configuration("runtime.name=test\n");
    if (!cfg || !host.configure(cfg.value())) { std::fprintf(stderr, "configure_host failed\n"); std::exit(1); }
}

} // namespace kritva::runtime::test