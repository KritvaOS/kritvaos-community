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
    /// (empty name, tick_ms 100). Valid configuration is required only if the
    /// application calls configure().
    [[nodiscard]] const RuntimeSettings& settings() const noexcept { return settings_; }

    /// Each operation forwards to Core and returns its Result unchanged;
    /// an operation invalid for the current state fails with INVALID_STATE.
    core::Result<void> initialize() { return manager_.initialize(); }  ///< -> READY
    core::Result<void> start() { return manager_.start(); }            ///< READY -> RUNNING
    core::Result<void> stop() { return manager_.stop(); }              ///< -> STOPPED
    core::Result<void> shutdown() { return manager_.shutdown(); }      ///< releases components

    [[nodiscard]] core::LifecycleState state() const noexcept { return manager_.state(); }

    /// Drives the full lifecycle: initialize, start, stop, shutdown.
    ///
    /// `observer` (optional) sees the state after initialize (READY), start
    /// (RUNNING) and stop (STOPPED). INITIALIZING and STOPPING are transient
    /// inside Core and are not observable once an operation returns.
    ///
    /// If a step fails, the host performs controlled cleanup (Core reset(),
    /// FAULT -> STOPPED, then shutdown) and returns the original error; there
    /// is no retry or recovery. On success the host is left STOPPED and shut down.
    core::Result<void> run(const StateObserver& observer = {});

private:
    core::runtime::RuntimeManager manager_;
    RuntimeSettings settings_;
};

} // namespace kritva::runtime
