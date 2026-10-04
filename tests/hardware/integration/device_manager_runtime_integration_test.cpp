//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device_manager_runtime_integration_test.cpp
// Description : Integration: DeviceManager inside the unchanged KOS-I2 RuntimeHost.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-004; DER-205; DER-401..407; DER-607; DER-704
// API         : HARDWARE-MANAGER-IT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <memory>
#include <string>
#include <vector>

#include "../../runtime/check.hpp"
#include "../../runtime/probe_component.hpp"
#include "../support/test_endpoints.hpp"

#include <kritva/hardware/device_manager.hpp>
#include <kritva/runtime/runtime_host.hpp>

using namespace kritva;
using namespace kritva::hardware;
using namespace kritva::hardware::test;
using kritva::core::ErrorCode;
using kritva::core::EventType;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
using Log = std::vector<std::string>;

struct World {
    Log log;
    Device imu{DeviceInfo::create(DeviceId{1}, "imu").value()};
    Device motor{DeviceInfo::create(DeviceId{2}, "motor").value()};
    TestSensor* accel{};
    TestSensor* gyro{};
    TestActuator* command{};
    DeviceManager manager{core::runtime::ComponentId{100}};
    runtime::RuntimeHost host;
    runtime::EventLog events;
    runtime::test::ProbeComponent platform{7, "platform", host.runtime(), log};   // the manager depends on it

    World() {
        accel = add<TestSensor>(imu, 1, "accel");
        gyro = add<TestSensor>(imu, 2, "gyro");
        command = add<TestActuator>(motor, 1, "command");
        KRITVA_CHECK(manager.register_device(imu).has_value() && manager.register_device(motor).has_value());
        manager.set_event_sink(&events);
        host.set_event_sink(&events);
        KRITVA_CHECK(host.add_component(platform).has_value());
        KRITVA_CHECK(host.add_component(manager, {platform.info().id()}).has_value());
    }
    template <class T>
    T* add(Device& d, std::uint64_t id, const char* name) {
        auto ep = std::make_unique<T>(id, name);
        ep->log = &log;
        T* raw = ep.get();
        KRITVA_CHECK(d.add_endpoint(std::move(ep)).has_value());
        return raw;
    }
    // The loader allow-list: the runtime keys plus everything the devices accept.
    std::vector<std::string> allowed() const {
        auto keys = manager.config_keys();
        keys.push_back("runtime.name");
        return keys;
    }
    void configure(const char* text) {
        const auto cfg = runtime::parse_configuration(text, allowed());
        KRITVA_CHECK(cfg.has_value());
        KRITVA_CHECK(host.configure(cfg.value()).has_value());
    }
};

static void test_lifecycle_order_through_the_runtime() {
    World w;
    w.configure("runtime.name=hw\nimu.gyro.limit=4\n");
    KRITVA_CHECK(w.gyro->limit == 4);                                           // configuration reached the endpoint
    KRITVA_CHECK(w.host.run().has_value() && w.host.state() == LifecycleState::STOPPED);
    // The dependency (platform) comes up first and goes down last; the device manager fans out inside its slot.
    const Log expected{"accel.configure", "gyro.configure", "command.configure",
                       "platform.initialize:INITIALIZING", "accel.initialize", "gyro.initialize", "command.initialize",
                       "platform.start:READY", "accel.start", "gyro.start", "command.start",
                       "command.stop", "gyro.stop", "accel.stop", "platform.stop:STOPPING",
                       "command.shutdown", "gyro.shutdown", "accel.shutdown", "platform.shutdown:STOPPED"};
    KRITVA_CHECK(w.log == expected);
}

static void test_allow_list_rejects_typos() {
    World w;
    const auto typo = runtime::parse_configuration("runtime.name=hw\nimu.gyro.limt=4\n", w.allowed());
    KRITVA_CHECK(!typo.has_value() && typo.error().message == "line 2: unknown key 'imu.gyro.limt'");
}

static void test_endpoint_failure_is_observable_and_shutdown_works() {
    World w;
    w.configure("runtime.name=hw\n");
    KRITVA_CHECK(w.host.initialize().has_value() && w.host.start().has_value());
    w.events.clear();
    KRITVA_CHECK(w.gyro->inject_fault("gyro lost").has_value());

    const auto report = w.host.failure_report().value();
    KRITVA_CHECK(report.any() && report.failed.size() == 1 && report.failed[0] == w.manager.info().id());
    const auto obs = w.host.observe({{w.manager.info().id(), &w.manager}}).value();
    bool found = false;
    for (const auto& c : obs.components) {
        if (c.name != "device_manager") continue;
        found = true;
        KRITVA_CHECK(c.observation.health.state() == HealthState::UNHEALTHY && c.observation.health.detail() == "imu.gyro: gyro lost");
        KRITVA_CHECK(c.observation.status.code() == core::StatusCode::FAILED && c.observation.lifecycle == LifecycleState::RUNNING);
    }
    KRITVA_CHECK(found);
    const auto events = w.events.snapshot();
    KRITVA_CHECK(events.size() == 1 && events[0].type == EventType::ERROR && events[0].source_id == w.manager.info().id());

    KRITVA_CHECK(w.host.controlled_shutdown().has_value());
    KRITVA_CHECK(w.host.state() == LifecycleState::STOPPED);
    for (auto* e : {static_cast<Endpoint*>(w.accel), static_cast<Endpoint*>(w.gyro), static_cast<Endpoint*>(w.command)}) {
        KRITVA_CHECK(e->lifecycle_state() == LifecycleState::STOPPED);                          // faulted one released too
    }
    // The endpoint fault was the only ERROR the manager is the source of.
    std::size_t errors_from_manager = 0;
    for (const auto& e : w.events.snapshot()) if (e.type == EventType::ERROR && e.source_id == w.manager.info().id()) ++errors_from_manager;
    KRITVA_CHECK(errors_from_manager == 1);
}

static void test_endpoint_initialize_failure_through_run() {
    World w;
    w.configure("runtime.name=hw\n");
    w.gyro->fail_at = FailAt::INITIALIZE;
    const auto r = w.host.run();
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INTERNAL_ERROR && r.error().source == w.manager.info().id());
    KRITVA_CHECK(w.host.state() == LifecycleState::STOPPED);                                    // controlled cleanup
    KRITVA_CHECK(w.accel->lifecycle_state() == LifecycleState::STOPPED && w.command->lifecycle_state() == LifecycleState::UNKNOWN);
}

static void test_explicit_restart_after_failure() {
    World w;
    w.configure("runtime.name=hw\n");
    KRITVA_CHECK(w.host.initialize().has_value() && w.host.start().has_value());
    KRITVA_CHECK(w.gyro->inject_fault().has_value() && w.host.controlled_shutdown().has_value());
    KRITVA_CHECK(w.host.initialize().has_value() && w.host.start().has_value());                // the operator's explicit choice
    KRITVA_CHECK(!w.host.failure_report().value().any());
    TestSample s;
    KRITVA_CHECK(w.gyro->read(s).has_value());
    KRITVA_CHECK(w.host.controlled_shutdown().has_value());
}

int main() {
    test_lifecycle_order_through_the_runtime();
    test_allow_list_rejects_typos();
    test_endpoint_failure_is_observable_and_shutdown_works();
    test_endpoint_initialize_failure_through_run();
    test_explicit_restart_after_failure();
    std::printf("device_manager_runtime_integration_test: PASS\n");
    return 0;
}
