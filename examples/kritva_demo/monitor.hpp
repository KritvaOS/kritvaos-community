//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : monitor.hpp
// Description : Demo Monitor: observes runtime health through the host's failure report.
//
// Component   : KritvaOS Demo
// Module      : Reference Demo
// Layer       : Application
//
// Requirements: RR-OBS-001..007; RR-FLT-002
// API         : kritva::demo::Monitor
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <cstdint>
#include <vector>

#include <kritva/runtime/runtime_host.hpp>

#include "demo_component.hpp"

namespace kritva::demo {

/// Software-only monitor. Each tick reads the host's failure report (read-only
/// observation) and remembers the latest result. It never changes other
/// components. Depends on the sensor and the controller.
class Monitor final : public DemoComponent, public core::runtime::IComponentStatistics {
public:
    static constexpr std::uint64_t kId = 3;
    explicit Monitor(const runtime::RuntimeHost& host) : DemoComponent(kId, "monitor"), host_(host) {}

    /// One observation; a no-op unless RUNNING.
    void tick();

    [[nodiscard]] bool failure_observed() const noexcept { return !failed_.empty(); }
    [[nodiscard]] const std::vector<core::runtime::ComponentId>& failed() const noexcept { return failed_; }
    [[nodiscard]] const std::vector<core::runtime::ComponentId>& affected() const noexcept { return affected_; }
    [[nodiscard]] core::Statistics statistics() const override { return stats_; }

private:
    const runtime::RuntimeHost& host_;
    std::vector<core::runtime::ComponentId> failed_;
    std::vector<core::runtime::ComponentId> affected_;
    core::Statistics stats_;
};

} // namespace kritva::demo
