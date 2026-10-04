//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : fault_observation_integration_test.cpp
// Description : Integration: endpoint fault evidence through the DeviceManager, the I2 observation and a full deterministic scenario.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-601..608; DER-701..705; DER-704
// API         : HARDWARE-FAULT-IT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <string>
#include <vector>

#include "../../runtime/check.hpp"

#include <kritva/hardware/device_manager.hpp>
#include <kritva/hardware/mock/mock_imu.hpp>
#include <kritva/hardware/mock/mock_motor.hpp>
#include <kritva/runtime/runtime_host.hpp>

using namespace kritva;
using namespace kritva::hardware;
using namespace kritva::hardware::mock;
using kritva::core::EventType;
using kritva::core::HealthState;
using kritva::core::LifecycleState;

struct Evidence {
    std::string device_text;       // describe() of the device diagnostics
    std::string runtime_text;      // I2 describe() of the runtime observation
    std::vector<int> event_types;
    bool failure_reported{false};
    std::string final_state;
};

static Evidence scenario() {
    MockImuDevice imu(DeviceInfo::create(DeviceId{1}, "left_arm_imu").value());
    MockMotorDevice motor(DeviceInfo::create(DeviceId{2}, "shoulder_motor").value());
    DeviceManager manager{core::runtime::ComponentId{100}};
    KRITVA_CHECK(manager.register_device(imu).has_value() && manager.register_device(motor).has_value());
    runtime::RuntimeHost host;
    runtime::EventLog events;
    host.set_event_sink(&events);
    manager.set_event_sink(&events);
    KRITVA_CHECK(host.add_component(manager).has_value());
    auto keys = manager.config_keys();
    keys.push_back("runtime.name");
    KRITVA_CHECK(host.configure(runtime::parse_configuration("runtime.name=obs\nleft_arm_imu.acceleration.fault_after_ops=3\n", keys).value()).has_value());
    KRITVA_CHECK(host.initialize().has_value() && host.start().has_value());

    AccelerationSample a;
    AngularVelocitySample w;
    for (int i = 0; i < 3; ++i) { KRITVA_CHECK(imu.acceleration().read(a).has_value()); KRITVA_CHECK(imu.angular_velocity().read(w).has_value()); }
    KRITVA_CHECK(motor.command().write(MotorCommand{0.5}).has_value());
    KRITVA_CHECK(!imu.acceleration().read(a).has_value());                                   // faulted by the schedule: refused
    for (int i = 0; i < 3; ++i) KRITVA_CHECK(!imu.acceleration().read(a).has_value());       // and it stays refused

    Evidence e;
    e.failure_reported = host.failure_report().value().any();
    e.device_text = describe(manager.diagnostics());
    e.runtime_text = runtime::describe(host.observe({{manager.info().id(), &manager}}).value());
    KRITVA_CHECK(host.controlled_shutdown().has_value());
    for (const auto& ev : events.snapshot()) e.event_types.push_back(static_cast<int>(ev.type));
    e.final_state = std::string(runtime::to_string(host.state()));
    return e;
}

int main() {
    const Evidence e = scenario();

    // Device-level evidence.
    KRITVA_CHECK(e.device_text.find("  endpoint acceleration (id=1) sensor state=FAULT status=FAILED health=UNHEALTHY "
                                    "detail=\"injected fault after the scheduled operations\" ok=3 failed=4 ") != std::string::npos);
    KRITVA_CHECK(e.device_text.find("last_error=RESOURCE_UNAVAILABLE") != std::string::npos);
    KRITVA_CHECK(e.device_text.find("device left_arm_imu (id=1) enabled=true status=FAILED health=UNHEALTHY") != std::string::npos);
    KRITVA_CHECK(e.device_text.find("  endpoint angular_velocity (id=2) sensor state=RUNNING status=OK health=HEALTHY") != std::string::npos);
    KRITVA_CHECK(e.device_text.find("device shoulder_motor (id=2) enabled=true status=OK health=HEALTHY") != std::string::npos);

    // The unchanged I2 observation shows the same failure at component level.
    KRITVA_CHECK(e.failure_reported);
    KRITVA_CHECK(e.runtime_text.find("component device_manager (id=100) state=RUNNING health=UNHEALTHY status=6 "
                                     "detail=\"left_arm_imu.acceleration: injected fault after the scheduled operations\"") != std::string::npos);
    KRITVA_CHECK(e.runtime_text.find("samples=") != std::string::npos);                       // aggregated statistics provider

    // Exactly one ERROR event for the endpoint fault (no event for the refused reads), then shutdown.
    int errors = 0;
    for (int t : e.event_types) if (t == static_cast<int>(EventType::ERROR)) ++errors;
    KRITVA_CHECK(errors == 1 && e.final_state == "STOPPED");

    // Deterministic failure evidence: a second identical run produces identical text and events.
    const Evidence again = scenario();
    KRITVA_CHECK(again.device_text == e.device_text && again.runtime_text == e.runtime_text && again.event_types == e.event_types);

    std::printf("fault_observation_integration_test: PASS\n");
    return 0;
}
