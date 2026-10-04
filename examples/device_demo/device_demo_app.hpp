//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device_demo_app.hpp
// Description : Reference Device/Endpoint application over mock hardware.
//
// Component   : KritvaOS Demo
// Module      : Device Demo
// Layer       : Application
//
// Requirements: DER-804; DER-401..407; DER-607; DER-703; DER-704
// API         : kritva::demo::DeviceDemoApplication
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <ostream>
#include <string>
#include <vector>

#include <kritva/core/core.hpp>

namespace kritva::demo {

enum class DeviceDemoOutcome {
    COMPLETED,   ///< The scenario ran and every expectation held (see DeviceDemoApplication).
    ERROR,       ///< A configuration, lifecycle or expectation error; the runtime was shut down.
};

/// Process exit code: COMPLETED 0, ERROR 1.
[[nodiscard]] constexpr int exit_code(DeviceDemoOutcome outcome) noexcept {
    return outcome == DeviceDemoOutcome::COMPLETED ? 0 : 1;
}

/// Every configuration key the demo understands (runtime, demo and all mock device keys),
/// for the loader allow-list so that typos are rejected.
[[nodiscard]] std::vector<std::string> device_demo_config_keys();

/// The KOS-I3 reference application: a RuntimeHost with one DeviceManager over mock hardware.
///
///   Runtime
///    |- left_arm_imu     (acceleration, angular_velocity)
///    `- shoulder_motor   (command, position)
///
/// Flow: configure, register the devices, discover every endpoint by name, initialize, start,
/// run `demo.ticks` deterministic steps (read the IMU and the joint position, write one motor
/// command), print status, health, capabilities and statistics, then (if `demo.inject_fault`)
/// fault the `angular_velocity` endpoint on tick `demo.fault_after_tick`, verify that the
/// failure is observed, that the healthy endpoints keep working and that the faulted one does
/// NOT recover, and finally perform a controlled shutdown and verify STOPPED.
///
/// Configuration keys: runtime.name (required), demo.ticks (1..1000, default 5),
/// demo.inject_fault (bool, default true), demo.fault_after_tick (1..demo.ticks, default 3),
/// demo.command_milli_rad_s (motor command in 0.001 rad/s, default 500), plus the device keys
/// (`<device>.enabled`, `<device>.<endpoint>.<setting>`). No wall clock, threads or sleeping:
/// the same configuration always produces the same output.
class DeviceDemoApplication {
public:
    explicit DeviceDemoApplication(std::ostream& out) : out_(out) {}
    [[nodiscard]] DeviceDemoOutcome run(const core::Configuration& configuration);

private:
    std::ostream& out_;
};

} // namespace kritva::demo
