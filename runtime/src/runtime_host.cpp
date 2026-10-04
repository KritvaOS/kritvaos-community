//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : runtime_host.cpp
// Description : Application runtime host implementation.
//
// Component   : KritvaOS Runtime
// Module      : Runtime Host
// Layer       : Application Runtime
//
// Requirements: RR-LIF-001..008; RR-CMP-002; RR-DEP-001; RR-CFG-001..005; RR-OBS-001..007; RR-FLT-001..007
// API         : kritva::runtime::RuntimeHost
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <kritva/runtime/runtime_host.hpp>

#include <set>

namespace kritva::runtime {

using core::LifecycleState;
using core::Result;

std::string_view to_string(LifecycleState state) noexcept {
    switch (state) {
        case LifecycleState::UNKNOWN:      return "UNKNOWN";
        case LifecycleState::INITIALIZING: return "INITIALIZING";
        case LifecycleState::READY:        return "READY";
        case LifecycleState::RUNNING:      return "RUNNING";
        case LifecycleState::STOPPING:     return "STOPPING";
        case LifecycleState::STOPPED:      return "STOPPED";
        case LifecycleState::FAULT:        return "FAULT";
        case LifecycleState::RECOVERING:   return "RECOVERING";
    }
    return "INVALID";
}

void RuntimeHost::emit(core::EventType type, core::ErrorSeverity severity, core::Id source) const {
    if (sink_ == nullptr) return;
    core::Event event;
    event.type = type;
    event.severity = severity;
    event.source_id = source;
    (void)sink_->report(event);   // observation must never change lifecycle behavior
}

// Reports the outcome of one step as an Event and returns the result unchanged.
Result<void> RuntimeHost::report(core::EventType type, Result<void> result) const {
    if (result) emit(type, core::ErrorSeverity::INFO, core::Id{});
    else emit(type == core::EventType::CONFIGURATION ? type : core::EventType::ERROR,
              result.error().severity, result.error().source);
    return result;
}

Result<void> RuntimeHost::add_component(core::runtime::Component& component,
                                        std::initializer_list<core::runtime::ComponentId> depends_on) {
    if (auto r = manager_.register_component(component); !r) return r;
    for (const auto dependency : depends_on) {
        if (auto r = manager_.add_dependency(component.info().id(), dependency); !r) return r;
    }
    return Result<void>::success();
}

Result<void> RuntimeHost::configure(const core::Configuration& configuration) {
    auto settings = read_runtime_settings(configuration);
    if (!settings) return report(core::EventType::CONFIGURATION, Result<void>::failure(settings.error()));
    if (auto r = manager_.configure(configuration); !r) return report(core::EventType::CONFIGURATION, r);
    settings_ = std::move(settings).value();
    return report(core::EventType::CONFIGURATION, Result<void>::success());
}

Result<void> RuntimeHost::initialize() { return report(core::EventType::LIFECYCLE, manager_.initialize()); }
Result<void> RuntimeHost::start() { return report(core::EventType::LIFECYCLE, manager_.start()); }
Result<void> RuntimeHost::stop() { return report(core::EventType::LIFECYCLE, manager_.stop()); }
Result<void> RuntimeHost::shutdown() { return report(core::EventType::LIFECYCLE, manager_.shutdown()); }

Result<RuntimeObservation> RuntimeHost::observe(const StatisticsProviders& providers) const {
    auto order = manager_.component_order();
    if (!order) return Result<RuntimeObservation>::failure(order.error());

    RuntimeObservation snapshot;
    snapshot.state = manager_.state();
    snapshot.statistics = manager_.statistics();
    if (const core::Error* fault = manager_.fault_error()) snapshot.fault = *fault;
    for (const auto id : order.value()) {
        const core::runtime::Component* component = manager_.registry().find(id);
        const auto it = providers.find(id);
        snapshot.components.push_back(ComponentRecord{
            component->info().name(),
            core::runtime::observe(*component, it == providers.end() ? nullptr : it->second)});
    }
    return Result<RuntimeObservation>::success(std::move(snapshot));
}

Result<FailureReport> RuntimeHost::failure_report() const {
    const auto observation = observe();
    if (!observation) return Result<FailureReport>::failure(observation.error());

    FailureReport report;
    std::set<core::runtime::ComponentId> bad;   // failed or affected: used to propagate down the order
    for (const auto& c : observation.value().components) {
        const auto& o = c.observation;
        if (o.lifecycle == LifecycleState::FAULT || o.health.state() == core::HealthState::UNHEALTHY) {
            report.failed.push_back(o.id);
            bad.insert(o.id);
            continue;
        }
        // Dependencies come first in the order, so one pass reaches every transitive dependent.
        for (const auto dependency : manager_.dependencies().dependencies_of(o.id)) {
            if (bad.count(dependency) != 0) {
                report.affected.push_back(o.id);
                bad.insert(o.id);
                break;
            }
        }
    }
    return Result<FailureReport>::success(std::move(report));
}

Result<void> RuntimeHost::controlled_shutdown() {
    const LifecycleState s = manager_.state();
    if (s == LifecycleState::READY || s == LifecycleState::RUNNING) {
        (void)stop();   // a failed component rejects stop(); the failure is already an ERROR event
    }
    if (manager_.state() == LifecycleState::FAULT) {
        if (auto r = report(core::EventType::LIFECYCLE, manager_.reset()); !r) return r;
    }
    return shutdown();
}

Result<void> RuntimeHost::run(const StateObserver& observer) {
    const auto notify = [&] { if (observer) observer(manager_.state()); };
    const auto fail = [&](Result<void> failed) {
        (void)controlled_shutdown();
        return failed;
    };

    if (auto r = initialize(); !r) return fail(r);
    notify();
    if (auto r = start(); !r) return fail(r);
    notify();
    if (auto r = stop(); !r) return fail(r);
    notify();
    return shutdown();
}

} // namespace kritva::runtime
