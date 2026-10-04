//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : observation.cpp
// Description : Runtime observation implementation.
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

#include <kritva/runtime/observation.hpp>

#include <kritva/runtime/runtime_host.hpp>

namespace kritva::runtime {

namespace {

const char* name_of(core::HealthState s) {
    switch (s) {
        case core::HealthState::UNKNOWN:   return "UNKNOWN";
        case core::HealthState::HEALTHY:   return "HEALTHY";
        case core::HealthState::DEGRADED:  return "DEGRADED";
        case core::HealthState::UNHEALTHY: return "UNHEALTHY";
    }
    return "INVALID";
}

} // namespace

core::Result<void> EventLog::report(const core::Event& event) {
    core::Event stored = event;
    if (!stored.event_id.valid()) stored.event_id = core::Id{next_id_};
    ++next_id_;
    if (events_.size() == capacity_) {
        events_.pop_front();
        ++dropped_;
    }
    events_.push_back(stored);
    return core::Result<void>::success();
}

std::string describe(const RuntimeObservation& o) {
    std::string out = "runtime state=" + std::string(to_string(o.state)) +
                      " ok_ops=" + std::to_string(o.statistics.sample_count.value()) +
                      " failed_ops=" + std::to_string(o.statistics.error_count.value()) + "\n";
    if (o.fault) out += "runtime fault: " + o.fault->message + "\n";
    for (const auto& c : o.components) {
        const auto& obs = c.observation;
        out += "component " + c.name + " (id=" + std::to_string(obs.id.value()) + ")" +
               " state=" + std::string(to_string(obs.lifecycle)) +
               " health=" + name_of(obs.health.state()) +
               " status=" + std::to_string(static_cast<int>(obs.status.code()));
        if (!obs.health.detail().empty()) out += " detail=\"" + obs.health.detail() + "\"";
        if (obs.statistics) {
            out += " samples=" + std::to_string(obs.statistics->sample_count.value()) +
                   " errors=" + std::to_string(obs.statistics->error_count.value());
        }
        out += "\n";
    }
    return out;
}

} // namespace kritva::runtime
