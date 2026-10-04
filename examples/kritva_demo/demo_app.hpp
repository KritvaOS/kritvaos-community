//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : demo_app.hpp
// Description : Reference application: composes Sensor, Controller and Monitor over the runtime host.
//
// Component   : KritvaOS Demo
// Module      : Reference Demo
// Layer       : Application
//
// Requirements: RR-APP-001..006; RR-TST-007
// API         : kritva::demo::DemoApplication
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <ostream>

#include <kritva/core/core.hpp>

namespace kritva::demo {

enum class DemoOutcome {
    CLEAN,             ///< Ran to completion and shut down normally.
    FAILURE_HANDLED,   ///< A component failure was observed and handled by controlled shutdown.
    ERROR,             ///< Configuration or lifecycle error; the runtime was shut down.
};

/// Process exit code of an outcome: CLEAN 0, ERROR 1, FAILURE_HANDLED 3.
[[nodiscard]] constexpr int exit_code(DemoOutcome outcome) noexcept {
    return outcome == DemoOutcome::CLEAN ? 0 : outcome == DemoOutcome::ERROR ? 1 : 3;
}

/// The reference application. Flow: configure, compose, initialize (READY),
/// start (RUNNING), run `demo.ticks` deterministic simulated steps (no sleeping,
/// no threads), observe, then either stop normally or, if the Monitor sees a
/// failed component, report it and perform a controlled shutdown.
///
/// Configuration (key=value; see kritva_demo.conf): runtime.name (required),
/// runtime.tick_ms, demo.ticks (1..10000, default 5), `<component>.enabled`
/// (default true) for sensor/controller/monitor, sensor.failure_after_ticks,
/// controller.gain. A disabled Sensor with an enabled Controller is a missing
/// dependency and ends as ERROR.
///
/// All progress is written to `out`; the same configuration always produces
/// the same output.
class DemoApplication {
public:
    explicit DemoApplication(std::ostream& out) : out_(out) {}
    [[nodiscard]] DemoOutcome run(const core::Configuration& configuration);

private:
    std::ostream& out_;
};

} // namespace kritva::demo
