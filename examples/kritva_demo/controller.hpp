//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : controller.hpp
// Description : Demo Controller: derives a command from the sensor reading.
//
// Component   : KritvaOS Demo
// Module      : Reference Demo
// Layer       : Application
//
// Requirements: RR-APP-002; RR-DEP-001
// API         : kritva::demo::Controller
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <cstdint>

#include <kritva/runtime/configuration.hpp>

#include "demo_component.hpp"
#include "sensor.hpp"

namespace kritva::demo {

/// Software-only controller. command = gain * sensor reading; computed only
/// while both it and the sensor are RUNNING. Configuration: `controller.gain`
/// (int 1..100, default 2). Depends on the sensor.
class Controller final : public DemoComponent, public core::runtime::IComponentStatistics {
public:
    static constexpr std::uint64_t kId = 2;
    explicit Controller(const Sensor& sensor) : DemoComponent(kId, "controller"), sensor_(sensor) {}

    /// One simulated step; returns true if a command was produced.
    bool tick();

    [[nodiscard]] std::int64_t command() const noexcept { return command_; }
    [[nodiscard]] std::int64_t gain() const noexcept { return gain_; }
    [[nodiscard]] core::Statistics statistics() const override { return stats_; }

protected:
    core::Result<void> on_configure(const core::Configuration& cfg) override;

private:
    const Sensor& sensor_;
    std::int64_t gain_{2};
    std::int64_t command_{0};
    core::Statistics stats_;
};

} // namespace kritva::demo
