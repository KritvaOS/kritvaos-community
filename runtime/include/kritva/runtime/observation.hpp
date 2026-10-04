//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : observation.hpp
// Description : Runtime observation: state/health/statistics snapshot, bounded event log, diagnostics text.
//
// Component   : KritvaOS Runtime
// Module      : Observation
// Layer       : Application Runtime
//
// Requirements: RR-OBS-001..007; RR-PERF-002
// API         : kritva::runtime::RuntimeObservation
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <cstddef>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <kritva/core/core.hpp>

namespace kritva::runtime {

using StatisticsProviders = std::map<core::runtime::ComponentId, const core::runtime::IComponentStatistics*>;

/// One component's observation (Core's ComponentObservation) plus its name.
struct ComponentRecord {
    std::string name;
    core::runtime::ComponentObservation observation;
};

/// A detached snapshot of the runtime, read from the live objects when taken
/// (no mirrored state), so it is consistent with the actual runtime (RR-OBS-007).
struct RuntimeObservation {
    core::LifecycleState state{core::LifecycleState::UNKNOWN};
    core::Statistics statistics;                 ///< Core runtime statistics (successful / failed component operations).
    std::optional<core::Error> fault;            ///< The Error that put the runtime into FAULT, if it is in FAULT.
    std::vector<ComponentRecord> components;     ///< In dependency order (dependencies first).
};

/// Which components are failed and which depend on a failed one (RR-FLT-001..003, RR-DEP-004).
/// Read from the live components, not stored; informational only: reporting a
/// failure never changes any component or runtime state (no silent recovery).
struct FailureReport {
    std::vector<core::runtime::ComponentId> failed;    ///< lifecycle FAULT or health UNHEALTHY, dependency order.
    std::vector<core::runtime::ComponentId> affected;  ///< transitive dependents of a failed component (not failed themselves).
    [[nodiscard]] bool any() const noexcept { return !failed.empty(); }
};

/// Bounded in-memory log of Core Events (RR-PERF-002: no uncontrolled growth).
///
/// Implements Core's IEventSink: components report through Core's
/// ComponentEventReporter, the host reports runtime events. The log assigns a
/// sequential `event_id` (starting at 1) when the event has none. When full,
/// the oldest event is dropped and counted in dropped().
///
/// Events with an invalid `source_id` come from the runtime itself.
/// Thread-safety: none. Control plane only.
class EventLog final : public core::runtime::IEventSink {
public:
    explicit EventLog(std::size_t capacity = 256) : capacity_(capacity == 0 ? 1 : capacity) {}

    core::Result<void> report(const core::Event& event) override;

    [[nodiscard]] std::vector<core::Event> snapshot() const { return {events_.begin(), events_.end()}; }
    [[nodiscard]] std::size_t size() const noexcept { return events_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::uint64_t dropped() const noexcept { return dropped_; }
    void clear() noexcept { events_.clear(); }

private:
    std::size_t capacity_;
    std::deque<core::Event> events_;
    std::uint64_t next_id_{1};
    std::uint64_t dropped_{0};
};

/// Diagnostics text of a snapshot (RR-OBS-006): runtime state and statistics,
/// the fault error if any, and one line per component with lifecycle, status
/// code and health (with its detail). Contains no configuration values.
[[nodiscard]] std::string describe(const RuntimeObservation& observation);

} // namespace kritva::runtime
