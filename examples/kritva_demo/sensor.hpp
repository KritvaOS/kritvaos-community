//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : sensor.hpp
// Description : Demo Sensor: produces a deterministic simulated reading per tick; can fail on demand.
//
// Component   : KritvaOS Demo
// Module      : Reference Demo
// Layer       : Application
//
// Requirements: RR-APP-006; RR-FLT-001..004
// API         : kritva::demo::Sensor
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <cstdint>

#include <kritva/runtime/configuration.hpp>

#include "demo_component.hpp"

namespace kritva::demo {

/// Software-only sensor (no hardware). Reading n is `n * 10`.
///
/// Configuration: `sensor.failure_after_ticks` (int 0..1000000, default 0 =
/// never): the tick on which the sensor fails itself. On failure the sensor
/// enters FAULT / UNHEALTHY and reports an ERROR event to the sink, if set.
class Sensor final : public DemoComponent, public core::runtime::IComponentStatistics {
public:
    static constexpr std::uint64_t kId = 1;
    Sensor() : DemoComponent(kId, "sensor") {}

    void set_event_sink(core::runtime::IEventSink* sink) noexcept { sink_ = sink; }

    /// One simulated step; a no-op unless RUNNING. Returns true if a reading was produced.
    bool tick();

    /// Fails the sensor now (used by the configured trigger and by tests).
    void inject_failure(const char* reason);

    [[nodiscard]] std::int64_t reading() const noexcept { return reading_; }
    [[nodiscard]] std::uint64_t ticks() const noexcept { return ticks_; }
    [[nodiscard]] core::Statistics statistics() const override { return stats_; }

protected:
    core::Result<void> on_configure(const core::Configuration& cfg) override;

private:
    std::int64_t failure_after_ticks_{0};
    std::int64_t reading_{0};
    std::uint64_t ticks_{0};
    core::Statistics stats_;
    core::runtime::IEventSink* sink_{nullptr};
};

} // namespace kritva::demo
