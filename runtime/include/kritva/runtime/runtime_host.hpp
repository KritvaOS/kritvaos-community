//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : runtime_host.hpp
// Description : Application runtime host: owns a Core RuntimeManager and drives its lifecycle.
//
// Component   : KritvaOS Runtime
// Module      : Runtime Host
// Layer       : Application Runtime
//
// Requirements: RR-LIF-001..008; RR-CMP-002; RR-DEP-001; RR-APP-001..004
// API         : kritva::runtime::RuntimeHost
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <functional>
#include <initializer_list>
#include <string_view>

#include <kritva/core/core.hpp>
#include <kritva/runtime/configuration.hpp>
#include <kritva/runtime/observation.hpp>

namespace kritva::runtime {

/// Human-readable name of a Core lifecycle state ("READY", "RUNNING", ...).
[[nodiscard]] std::string_view to_string(core::LifecycleState state) noexcept;

/// Application-level host of the Kritva runtime.
///
/// The host owns one kritva::core::runtime::RuntimeManager and adds only the
/// application glue: a single place to drive the lifecycle and to observe it.
/// Lifecycle rules (transition table, invalid-transition rejection, component
/// ordering) are Core's; the host does not re-implement them.
///
/// Thread-safety: none; the lifecycle is driven from one thread.
/// Real-time: control plane only (allocates, may block); not for RT paths.
class RuntimeHost {
public:
    /// Called with the lifecycle state reached after each lifecycle step.
    using StateObserver = std::function<void(core::LifecycleState)>;

    RuntimeHost() = default;
    RuntimeHost(const RuntimeHost&) = delete;
    RuntimeHost& operator=(const RuntimeHost&) = delete;

    /// The owned Core runtime, for registering components before initialize().
    [[nodiscard]] core::runtime::RuntimeManager& runtime() noexcept { return manager_; }
    [[nodiscard]] const core::runtime::RuntimeManager& runtime() const noexcept { return manager_; }

    /// Registers `component` and records that it depends on each id in
    /// `depends_on` (those components start before it and stop after it).
    /// The host does not own the component; it must outlive the host.
    ///
    /// Setup only: Core fixes the topology at the first successful
    /// initialize(). Registration and dependency errors are Core's, returned
    /// unchanged (duplicate identity, self/duplicate/cyclic dependency). On a
    /// dependency error the component stays registered with the dependencies
    /// added so far; a dependency on an unregistered id is reported by
    /// initialize() (CONFIGURATION_ERROR, runtime stays UNKNOWN).
    core::Result<void> add_component(core::runtime::Component& component,
                                     std::initializer_list<core::runtime::ComponentId> depends_on = {});

    /// Validates the runtime-level settings, then applies `configuration` to
    /// every component through Core (`RuntimeManager::configure`, dependency
    /// order). Valid only while UNKNOWN or STOPPED (Core rule). Fails with
    /// CONFIGURATION_ERROR on invalid settings and then changes nothing: no
    /// component is configured and `settings()` keeps its previous value.
    core::Result<void> configure(const core::Configuration& configuration);

    /// Settings of the last successful configure(); defaults before that
    /// (empty name, tick_ms 100). Requiring a valid configuration before
    /// operation (RR-CFG-003) is an application-level rule: the host does not
    /// refuse initialize() on an unconfigured host (the host is usable without
    /// configuration); DemoApplication enforces it by configuring first and
    /// never initializing after a configuration error.
    [[nodiscard]] const RuntimeSettings& settings() const noexcept { return settings_; }

    /// Optional event sink (not owned; must outlive the host or be cleared with
    /// nullptr). With a sink set, the host reports runtime-level Events, source
    /// id invalid (= the runtime): LIFECYCLE (INFO) after each successful
    /// initialize/start/stop/shutdown, CONFIGURATION (INFO) after a successful
    /// configure, and on any failed step an ERROR event (severity and source
    /// from the failing Error, CONFIGURATION type for configure failures).
    /// Components hand the same sink to Core's ComponentEventReporter.
    /// Core's Event carries no state payload: read the state with observe().
    void set_event_sink(core::runtime::IEventSink* sink) noexcept { sink_ = sink; }
    [[nodiscard]] core::runtime::IEventSink* event_sink() const noexcept { return sink_; }

    /// Snapshot of runtime state, statistics, fault and every component
    /// (state, status, health, optional statistics) in dependency order.
    /// Fails with the topology error (CONFIGURATION_ERROR) if a dependency is
    /// unregistered; nothing is invoked on components other than their
    /// read-only accessors.
    [[nodiscard]] core::Result<RuntimeObservation> observe(const StatisticsProviders& providers = {}) const;

    /// Components that are failed (lifecycle FAULT or health UNHEALTHY) and the
    /// components that depend on them. Deterministic; changes nothing. Fails
    /// like observe() if the topology is invalid.
    [[nodiscard]] core::Result<FailureReport> failure_report() const;

    /// Controlled shutdown, valid from any state, including after a failure
    /// (RR-FLT-006): stops the components if the runtime is READY/RUNNING
    /// (a failed component rejects stop(); that rejection is reported as a
    /// second ERROR event with the failed component as source, in addition to
    /// the event the component reported itself), performs Core's reset() if the runtime is in FAULT
    /// (FAULT -> STOPPED), then shuts every component down. Succeeds when the
    /// runtime is STOPPED and all components are released; there is no retry
    /// and no recovery: a failed component is never restarted.
    core::Result<void> controlled_shutdown();

    /// Each operation forwards to Core and returns its Result unchanged;
    /// an operation invalid for the current state fails with INVALID_STATE.
    core::Result<void> initialize();   ///< -> READY
    core::Result<void> start();        ///< READY -> RUNNING
    core::Result<void> stop();         ///< -> STOPPED
    core::Result<void> shutdown();     ///< releases components

    [[nodiscard]] core::LifecycleState state() const noexcept { return manager_.state(); }

    /// Drives the full lifecycle: initialize, start, stop, shutdown.
    ///
    /// `observer` (optional) sees the state after initialize (READY), start
    /// (RUNNING) and stop (STOPPED). INITIALIZING and STOPPING are transient
    /// inside Core and are not observable once an operation returns.
    ///
    /// If a step fails, the host performs controlled_shutdown() and returns the
    /// original error; there is no retry or recovery. On success the host is left STOPPED and shut down.
    core::Result<void> run(const StateObserver& observer = {});

private:
    void emit(core::EventType type, core::ErrorSeverity severity, core::Id source) const;
    core::Result<void> report(core::EventType type, core::Result<void> result) const;

    core::runtime::RuntimeManager manager_;
    RuntimeSettings settings_;
    core::runtime::IEventSink* sink_{nullptr};
};

} // namespace kritva::runtime
