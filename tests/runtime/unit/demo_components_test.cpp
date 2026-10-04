//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : demo_components_test.cpp
// Description : Unit tests of the demo Sensor, Controller and Monitor.
//
// Component   : KritvaOS Demo
// Module      : Tests
// Layer       : Application
//
// Requirements: RR-APP-002; RR-APP-006; RR-FLT-001..004; RR-TST-001
// API         : DEMO-COMPONENTS-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include "../check.hpp"

#include <kritva/runtime/runtime_host.hpp>

#include "controller.hpp"
#include "monitor.hpp"
#include "sensor.hpp"

using namespace kritva;
using namespace kritva::demo;
using kritva::core::ErrorCode;
using kritva::core::HealthState;
using kritva::core::LifecycleState;

static core::Configuration cfg(const char* text) { return runtime::parse_configuration(text).value(); }

static void test_sensor_readings_and_lifecycle() {
    Sensor s;
    KRITVA_CHECK(!s.tick());                                         // not RUNNING: no-op
    KRITVA_CHECK(s.start().error().code == ErrorCode::INVALID_STATE && s.start().error().source == s.info().id());
    KRITVA_CHECK(s.configure(cfg("")).has_value() && s.initialize().has_value() && s.start().has_value());
    KRITVA_CHECK(s.lifecycle_state() == LifecycleState::RUNNING && s.health().state() == HealthState::HEALTHY);
    KRITVA_CHECK(s.tick() && s.reading() == 10 && s.tick() && s.reading() == 20 && s.ticks() == 2);
    KRITVA_CHECK(s.statistics().sample_count.value() == 2);
    KRITVA_CHECK(s.configure(cfg("")).error().code == ErrorCode::INVALID_STATE);   // not while RUNNING
    KRITVA_CHECK(s.stop().has_value() && s.lifecycle_state() == LifecycleState::STOPPED);
    KRITVA_CHECK(s.shutdown().has_value());
}

static void test_sensor_configuration() {
    Sensor s;
    KRITVA_CHECK(s.configure(cfg("sensor.failure_after_ticks=-1")).error().code == ErrorCode::CONFIGURATION_ERROR);
    KRITVA_CHECK(s.configure(cfg("sensor.failure_after_ticks=abc")).error().source == s.info().id());
    KRITVA_CHECK(s.configure(cfg("sensor.failure_after_ticks=3")).has_value());
}

static void test_sensor_failure_after_ticks() {
    Sensor s;
    runtime::EventLog events;
    s.set_event_sink(&events);
    KRITVA_CHECK(s.configure(cfg("sensor.failure_after_ticks=2")).has_value());
    KRITVA_CHECK(s.initialize().has_value() && s.start().has_value());
    KRITVA_CHECK(s.tick() && s.lifecycle_state() == LifecycleState::RUNNING);
    KRITVA_CHECK(s.tick());                                          // tick 2 produces the reading, then fails
    KRITVA_CHECK(s.lifecycle_state() == LifecycleState::FAULT && s.health().state() == HealthState::UNHEALTHY);
    KRITVA_CHECK(s.health().detail() == "injected sensor failure");
    KRITVA_CHECK(s.status().code() == core::StatusCode::FAILED);
    KRITVA_CHECK(events.size() == 1 && events.snapshot()[0].type == core::EventType::ERROR
                 && events.snapshot()[0].source_id == s.info().id());
    KRITVA_CHECK(!s.tick());                                         // FAULT: no further readings
    KRITVA_CHECK(s.stop().error().code == ErrorCode::INVALID_STATE); // FAULT is left only by shutdown
    s.inject_failure("again");                                       // idempotent: no second event
    KRITVA_CHECK(events.size() == 1 && s.statistics().error_count.value() == 1);
    KRITVA_CHECK(s.shutdown().has_value() && s.lifecycle_state() == LifecycleState::STOPPED);
}

static void test_controller_command_and_configuration() {
    Sensor s;
    Controller c(s);
    KRITVA_CHECK(c.configure(cfg("controller.gain=0")).error().code == ErrorCode::CONFIGURATION_ERROR);
    KRITVA_CHECK(c.configure(cfg("controller.gain=101")).error().code == ErrorCode::CONFIGURATION_ERROR);
    KRITVA_CHECK(c.configure(cfg("controller.gain=3")).has_value() && c.gain() == 3);
    for (auto* x : {static_cast<core::runtime::Component*>(&s), static_cast<core::runtime::Component*>(&c)}) {
        KRITVA_CHECK(x->initialize().has_value() && x->start().has_value());
    }
    KRITVA_CHECK(s.tick() && c.tick() && c.command() == 30);          // gain 3 * reading 10
    s.inject_failure("x");
    KRITVA_CHECK(!c.tick() && c.command() == 30);                    // sensor not RUNNING: no new command
}

static void test_monitor_observes_failure() {
    runtime::RuntimeHost host;
    Sensor s;
    Monitor m(host);
    KRITVA_CHECK(host.add_component(s).has_value() && host.add_component(m, {s.info().id()}).has_value());
    KRITVA_CHECK(host.initialize().has_value() && host.start().has_value());
    m.tick();
    KRITVA_CHECK(!m.failure_observed() && m.statistics().sample_count.value() == 1);
    s.inject_failure("x");
    m.tick();
    KRITVA_CHECK(m.failure_observed() && m.failed().size() == 1 && m.failed()[0] == s.info().id());
    KRITVA_CHECK(m.affected().size() == 1 && m.affected()[0] == m.info().id());
    KRITVA_CHECK(m.lifecycle_state() == LifecycleState::RUNNING);    // observing does not change anything
    KRITVA_CHECK(host.controlled_shutdown().has_value());
}

int main() {
    test_sensor_readings_and_lifecycle();
    test_sensor_configuration();
    test_sensor_failure_after_ticks();
    test_controller_command_and_configuration();
    test_monitor_observes_failure();
    std::printf("demo_components_test: PASS\n");
    return 0;
}
