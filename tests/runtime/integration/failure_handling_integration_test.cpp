//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : failure_handling_integration_test.cpp
// Description : Integration: RUNNING -> injected failure -> event -> monitor observes -> controlled shutdown.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-FLT-001..006; RR-OBS-005
// API         : RUNTIME-FAILURE-IT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <string>
#include <vector>

#include "../check.hpp"
#include "../probe_component.hpp"

using namespace kritva;
using kritva::core::EventType;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
using kritva::runtime::EventLog;
using kritva::runtime::RuntimeHost;
using kritva::runtime::test::ProbeComponent;

// Sensor/controller/monitor-shaped flow with probes (the real demo components arrive in I2-006).
// The "monitor" is plain observation code: it only reads events and snapshots.
int main() {
    RuntimeHost host;
    EventLog events;
    host.set_event_sink(&events);
    std::vector<std::string> log;
    ProbeComponent sensor(1, "sensor", host.runtime(), log);
    ProbeComponent controller(2, "controller", host.runtime(), log);
    ProbeComponent monitor(3, "monitor", host.runtime(), log);
    sensor.set_event_sink(&events);
    KRITVA_CHECK(host.add_component(sensor).has_value());
    KRITVA_CHECK(host.add_component(controller, {sensor.info().id()}).has_value());
    KRITVA_CHECK(host.add_component(monitor, {sensor.info().id(), controller.info().id()}).has_value());

    KRITVA_CHECK(host.initialize().has_value() && host.start().has_value());
    KRITVA_CHECK(host.state() == LifecycleState::RUNNING && !host.failure_report().value().any());

    sensor.inject_failure();                                         // inject

    // Monitor-side observation of the failure.
    const auto report = host.failure_report().value();
    KRITVA_CHECK(report.any() && report.failed[0] == sensor.info().id());
    KRITVA_CHECK(report.affected.size() == 2);                       // controller and monitor depend on sensor
    bool error_event = false;
    for (const auto& e : events.snapshot()) {
        if (e.type == EventType::ERROR && e.source_id == sensor.info().id()) error_event = true;
    }
    KRITVA_CHECK(error_event);
    KRITVA_CHECK(host.observe().value().components[0].observation.health.state() == HealthState::UNHEALTHY);

    // Controlled shutdown.
    KRITVA_CHECK(host.controlled_shutdown().has_value());
    KRITVA_CHECK(host.state() == LifecycleState::STOPPED);
    KRITVA_CHECK(sensor.lifecycle_state() == LifecycleState::STOPPED);
    KRITVA_CHECK(controller.lifecycle_state() == LifecycleState::STOPPED);
    KRITVA_CHECK(monitor.lifecycle_state() == LifecycleState::STOPPED);
    std::printf("failure_handling_integration_test: PASS\n");
    return 0;
}
