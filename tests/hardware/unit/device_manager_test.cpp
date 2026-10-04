//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device_manager_test.cpp
// Description : Unit tests of the DeviceManager: ordering, configuration, failure, observation.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-205; DER-401..407; DER-601..607; DER-703; DER-704
// API         : HARDWARE-MANAGER-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <memory>
#include <string>
#include <vector>

#include "../../runtime/check.hpp"
#include "../support/test_endpoints.hpp"

#include <kritva/hardware/device_manager.hpp>

using namespace kritva::hardware;
using namespace kritva::hardware::test;
using kritva::core::ErrorCode;
using kritva::core::EventType;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
using kritva::core::StatusCode;
using Log = std::vector<std::string>;

// Collects events reported by the manager.
struct Sink final : kritva::core::runtime::IEventSink {
    std::vector<kritva::core::Event> events;
    kritva::core::Result<void> report(const kritva::core::Event& e) override { events.push_back(e); return kritva::core::Result<void>::success(); }
};

// imu: accel, gyro; motor: command, position; all log into `log`.
struct Rig {
    Log log;
    Device imu{DeviceInfo::create(DeviceId{1}, "imu").value()};
    Device motor{DeviceInfo::create(DeviceId{2}, "motor").value()};
    TestSensor* accel;
    TestSensor* gyro;
    TestActuator* command;
    TestSensor* position;
    DeviceManager manager{kritva::core::runtime::ComponentId{100}};
    Sink sink;

    Rig() {
        accel = add<TestSensor>(imu, 1, "accel");
        gyro = add<TestSensor>(imu, 2, "gyro");
        command = add<TestActuator>(motor, 1, "command");
        position = add<TestSensor>(motor, 2, "position");
        KRITVA_CHECK(manager.register_device(imu).has_value() && manager.register_device(motor).has_value());
        manager.set_event_sink(&sink);
    }
    template <class T>
    T* add(Device& d, std::uint64_t id, const char* name) {
        auto ep = std::make_unique<T>(id, name);
        ep->log = &log;
        T* raw = ep.get();
        KRITVA_CHECK(d.add_endpoint(std::move(ep)).has_value());
        return raw;
    }
    void run_up() { KRITVA_CHECK(manager.initialize().has_value() && manager.start().has_value()); }
};

static void test_component_identity() {
    Rig r;
    KRITVA_CHECK(r.manager.info().id() == kritva::core::runtime::ComponentId{100} && r.manager.info().name() == "device_manager");
    KRITVA_CHECK(r.manager.lifecycle_state() == LifecycleState::UNKNOWN && r.manager.status().code() == StatusCode::UNKNOWN);
}

static void test_deterministic_order() {                             // DER-405, DER-406
    Rig r;
    r.run_up();
    KRITVA_CHECK(r.manager.stop().has_value() && r.manager.shutdown().has_value());
    const Log expected{"accel.initialize", "gyro.initialize", "command.initialize", "position.initialize",
                       "accel.start", "gyro.start", "command.start", "position.start",
                       "position.stop", "command.stop", "gyro.stop", "accel.stop",
                       "position.shutdown", "command.shutdown", "gyro.shutdown", "accel.shutdown"};
    KRITVA_CHECK(r.log == expected);                                 // stop and shutdown are the exact reverse
}

static void test_registry_closes_at_initialize() {                   // DER-206
    Rig r;
    Device late(DeviceInfo::create(DeviceId{3}, "late").value());
    KRITVA_CHECK(r.manager.register_device(late).has_value());       // still open before initialize
    r.run_up();
    Device later(DeviceInfo::create(DeviceId{4}, "later").value());
    const auto res = r.manager.register_device(later);
    KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::INVALID_STATE && r.manager.registry().closed());
}

static void test_invalid_operations_fail_with_source() {
    Rig r;
    auto res = r.manager.start();                                    // before initialize
    KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::INVALID_STATE && res.error().source == r.manager.info().id());
    res = r.manager.stop();
    KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::INVALID_STATE);
    KRITVA_CHECK(r.manager.initialize().has_value());
    res = r.manager.initialize();                                    // twice
    KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::INVALID_STATE);
    res = r.manager.configure(kritva::core::Configuration{});        // not while live
    KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::INVALID_STATE);
    res = r.manager.shutdown();                                      // READY: stop first
    KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::INVALID_STATE);
    KRITVA_CHECK(r.manager.lifecycle_state() == LifecycleState::READY);
    for (auto* e : {static_cast<Endpoint*>(r.accel)}) KRITVA_CHECK(e->lifecycle_state() == LifecycleState::READY);   // untouched
}

static kritva::core::Configuration cfg(std::initializer_list<kritva::core::Parameter> params) {
    kritva::core::Configuration c;
    for (const auto& p : params) KRITVA_CHECK(c.set(p).has_value());
    return c;
}

static void test_config_keys_and_scoped_settings() {                 // DER-205
    Rig r;
    const auto keys = r.manager.config_keys();
    const std::vector<std::string> expected{"imu.enabled", "imu.accel.limit", "imu.gyro.limit",
                                            "motor.enabled", "motor.command.limit", "motor.position.limit"};
    KRITVA_CHECK(keys == expected);
    KRITVA_CHECK(r.manager.configure(cfg({{"imu.gyro.limit", std::int64_t{5}, {}}, {"motor.command.limit", std::int64_t{9}, {}},
                                          {"unrelated.key", std::int64_t{1}, {}}})).has_value());
    KRITVA_CHECK(!r.accel->limit && r.gyro->limit == 5 && r.command->limit == 9 && !r.position->limit);   // each gets only its own
    KRITVA_CHECK(r.manager.lifecycle_state() == LifecycleState::UNKNOWN);                                   // configure does not advance
}

static void test_configuration_errors() {
    Rig r;
    auto res = r.manager.configure(cfg({{"imu.enabled", std::string("yes"), {}}}));
    KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::CONFIGURATION_ERROR && res.error().source == r.manager.info().id());
    KRITVA_CHECK(r.gyro->configure_calls == 0);                                                              // nothing was applied
    r.gyro->fail_at = FailAt::CONFIGURE;
    res = r.manager.configure(cfg({}));
    KRITVA_CHECK(!res.has_value() && res.error().source == r.manager.info().id() && r.manager.lifecycle_state() == LifecycleState::UNKNOWN);
}

static void test_disabled_device_is_inert() {
    Rig r;
    KRITVA_CHECK(r.manager.configure(cfg({{"motor.enabled", false, {}}})).has_value());
    r.run_up();
    KRITVA_CHECK(r.accel->lifecycle_state() == LifecycleState::RUNNING && r.command->lifecycle_state() == LifecycleState::UNKNOWN);
    KRITVA_CHECK(r.manager.health().state() == HealthState::HEALTHY);
    KRITVA_CHECK(r.manager.capabilities().size() == 2);                                                      // the motor's are not offered
    KRITVA_CHECK(r.manager.stop().has_value() && r.manager.shutdown().has_value());
    KRITVA_CHECK(r.command->initialize_calls == 0 && r.command->shutdown_calls == 0);
}

static void test_failing_hooks_fault_the_manager() {                 // DER-404, DER-704
    for (auto at : {FailAt::INITIALIZE, FailAt::START}) {
        Rig r;
        r.gyro->fail_at = at;
        auto res = r.manager.initialize();
        if (res) res = r.manager.start();
        KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::INTERNAL_ERROR && res.error().source == r.manager.info().id());
        KRITVA_CHECK(r.manager.lifecycle_state() == LifecycleState::FAULT);
        KRITVA_CHECK(r.manager.health().state() == HealthState::UNHEALTHY && r.manager.status().code() == StatusCode::FAILED);
        KRITVA_CHECK(r.sink.events.empty());                                                                 // the call's Error is the report
        KRITVA_CHECK(r.manager.start().error().code == ErrorCode::INVALID_STATE);                            // FAULT is left only by shutdown
        r.gyro->fail_at = FailAt::NONE;
        KRITVA_CHECK(r.manager.shutdown().has_value() && r.manager.lifecycle_state() == LifecycleState::STOPPED);
        for (auto* e : {static_cast<Endpoint*>(r.accel), static_cast<Endpoint*>(r.gyro), static_cast<Endpoint*>(r.command), static_cast<Endpoint*>(r.position)}) {
            KRITVA_CHECK(e->lifecycle_state() != LifecycleState::READY && e->lifecycle_state() != LifecycleState::RUNNING);   // everything released
        }
    }
}

static void test_failing_stop_still_stops_every_endpoint() {          // M1: fail-safe, best effort
    for (int failing : {0, 1, 2, 3}) {                                                                      // reverse order: position, command, gyro, accel
        Rig r;
        r.run_up();
        Endpoint* const order[] = {r.position, r.command, r.gyro, r.accel};
        switch (failing) {
            case 0: r.position->fail_at = FailAt::STOP; break;
            case 1: r.command->fail_at = FailAt::STOP; break;
            case 2: r.gyro->fail_at = FailAt::STOP; break;
            default: r.accel->fail_at = FailAt::STOP; break;
        }
        r.log.clear();
        const auto res = r.manager.stop();
        KRITVA_CHECK(!res.has_value() && res.error().source == r.manager.info().id() && res.error().code == ErrorCode::INTERNAL_ERROR);
        KRITVA_CHECK(r.manager.lifecycle_state() == LifecycleState::FAULT);
        // Every endpoint was attempted, in reverse order, whatever failed.
        const Log attempts{"position.stop", "command.stop", "gyro.stop", "accel.stop"};
        KRITVA_CHECK(r.log == attempts);
        for (int i = 0; i < 4; ++i) {                                                                       // the failing one is FAULT, all others STOPPED
            KRITVA_CHECK(order[i]->lifecycle_state() == (i == failing ? LifecycleState::FAULT : LifecycleState::STOPPED));
        }
        // The actuator in particular is not left running.
        KRITVA_CHECK(r.command->lifecycle_state() != LifecycleState::RUNNING);
        KRITVA_CHECK(r.command->write(TestCommand{1}).error().code != ErrorCode::NONE && !r.command->write(TestCommand{1}).has_value());
        r.position->fail_at = r.command->fail_at = r.gyro->fail_at = r.accel->fail_at = FailAt::NONE;
        KRITVA_CHECK(r.manager.shutdown().has_value() && r.manager.lifecycle_state() == LifecycleState::STOPPED);
    }
}

static void test_shutdown_returns_the_first_stop_error() {            // M2
    Rig r;
    r.gyro->fail_at = FailAt::INITIALIZE;                                                                   // accel READY, gyro FAULT, others never started
    KRITVA_CHECK(!r.manager.initialize().has_value() && r.manager.lifecycle_state() == LifecycleState::FAULT);
    r.accel->fail_at = FailAt::STOP;                                                                        // releasing accel needs a stop that fails
    r.gyro->fail_at = FailAt::NONE;
    const auto res = r.manager.shutdown();
    KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::INTERNAL_ERROR && res.error().source == r.manager.info().id());
    KRITVA_CHECK(r.manager.lifecycle_state() == LifecycleState::FAULT);                                     // unchanged: a retry resumes
    KRITVA_CHECK(r.accel->lifecycle_state() == LifecycleState::STOPPED && r.gyro->lifecycle_state() == LifecycleState::STOPPED);   // everything was still released
    r.accel->fail_at = FailAt::NONE;
    KRITVA_CHECK(r.manager.shutdown().has_value() && r.manager.lifecycle_state() == LifecycleState::STOPPED);
}

static void test_failing_shutdown_hook_is_reported_and_retried() {    // M2: on_shutdown path
    Rig r;
    r.run_up();
    KRITVA_CHECK(r.manager.stop().has_value());
    r.command->fail_at = FailAt::SHUTDOWN;
    const auto res = r.manager.shutdown();
    KRITVA_CHECK(!res.has_value() && res.error().source == r.manager.info().id() && r.manager.lifecycle_state() == LifecycleState::STOPPED);
    KRITVA_CHECK(r.accel->shutdown_calls == 1 && r.gyro->shutdown_calls == 1 && r.position->shutdown_calls == 1 && r.command->shutdown_calls == 1);
    r.command->fail_at = FailAt::NONE;
    KRITVA_CHECK(r.manager.shutdown().has_value());                                                         // the retry resumes with the failing endpoint
    KRITVA_CHECK(r.command->shutdown_calls == 2 && r.accel->shutdown_calls == 1);                           // finished endpoints are not released twice
    KRITVA_CHECK(r.manager.shutdown().has_value() && r.command->shutdown_calls == 2);                       // and then it is a no-op
}

static void test_failed_configure_does_not_commit_device_switches() { // M3
    Rig r;
    r.command->fail_at = FailAt::CONFIGURE;                                                                 // motor is configured after imu
    auto res = r.manager.configure(cfg({{"imu.enabled", false, {}}, {"imu.accel.limit", std::int64_t{7}, {}}}));
    KRITVA_CHECK(!res.has_value() && res.error().source == r.manager.info().id() && r.manager.lifecycle_state() == LifecycleState::UNKNOWN);
    for (const auto& d : r.manager.diagnostics()) KRITVA_CHECK(d.enabled);                                  // the switch was NOT committed
    KRITVA_CHECK(r.accel->configure_calls == 0 && r.gyro->configure_calls == 0);                            // imu was disabled in this attempt: skipped
    // Without rollback: endpoints configured before the failing one keep their settings; later ones are untouched.
    Rig q;
    q.command->fail_at = FailAt::CONFIGURE;
    res = q.manager.configure(cfg({{"imu.accel.limit", std::int64_t{7}, {}}}));
    KRITVA_CHECK(!res.has_value() && q.accel->limit == 7 && q.accel->configure_calls == 1 && q.position->configure_calls == 0);
    // A malformed switch changes nothing at all.
    Rig w;
    res = w.manager.configure(cfg({{"imu.accel.limit", std::int64_t{7}, {}}, {"motor.enabled", std::string("no"), {}}}));
    KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::CONFIGURATION_ERROR && w.accel->configure_calls == 0);
    // A successful configure commits the switches.
    Rig ok;
    KRITVA_CHECK(ok.manager.configure(cfg({{"motor.enabled", false, {}}})).has_value());
    KRITVA_CHECK(ok.manager.diagnostics()[0].enabled && !ok.manager.diagnostics()[1].enabled);
}

static void test_shutdown_releases_devices_switched_off_after_the_run() {   // re-audit N3
    Rig r;
    r.run_up();
    KRITVA_CHECK(r.manager.stop().has_value());
    KRITVA_CHECK(r.manager.configure(cfg({{"imu.enabled", false, {}}})).has_value());     // reconfigured while STOPPED: imu is now off
    KRITVA_CHECK(r.manager.shutdown().has_value());
    for (auto* e : {static_cast<TestSensor*>(r.accel), r.gyro}) {
        KRITVA_CHECK(e->shutdown_calls == 1 && e->lifecycle_state() == LifecycleState::STOPPED);       // its live period was released
    }
    KRITVA_CHECK(r.command->shutdown_calls == 1 && r.position->shutdown_calls == 1);
    // A device that was never initialized is not touched.
    Rig q;
    KRITVA_CHECK(q.manager.configure(cfg({{"motor.enabled", false, {}}})).has_value());
    q.run_up();
    KRITVA_CHECK(q.manager.stop().has_value() && q.manager.shutdown().has_value());
    KRITVA_CHECK(q.command->shutdown_calls == 0 && q.command->initialize_calls == 0);
}

static void test_devices_are_sealed_at_initialize() {                  // m7
    Device empty(DeviceInfo::create(DeviceId{7}, "empty").value());
    Device off(DeviceInfo::create(DeviceId{8}, "off").value());
    DeviceManager manager{kritva::core::runtime::ComponentId{101}};
    KRITVA_CHECK(manager.register_device(empty).has_value() && manager.register_device(off).has_value());
    KRITVA_CHECK(!empty.sealed());
    KRITVA_CHECK(empty.add_endpoint(std::make_unique<TestSensor>(1, "early")).has_value());                // allowed before initialize
    KRITVA_CHECK(manager.configure(cfg({{"off.enabled", false, {}}})).has_value());
    KRITVA_CHECK(manager.initialize().has_value());
    KRITVA_CHECK(empty.sealed() && off.sealed());
    for (Device* d : {&empty, &off}) {
        const auto res = d->add_endpoint(std::make_unique<TestSensor>(9, "late"));                         // even a disabled or empty device
        KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::INVALID_STATE);
    }
    KRITVA_CHECK(manager.start().has_value() && manager.stop().has_value() && manager.shutdown().has_value());
    KRITVA_CHECK(manager.initialize().has_value());                                                          // a restart is unaffected
    empty.seal();                                                                                           // idempotent
    KRITVA_CHECK(empty.sealed());
}

static void test_a_destroyed_manager_does_not_remove_anothers_listener() {   // m6
    Device d(DeviceInfo::create(DeviceId{1}, "d").value());
    auto* e = static_cast<TestSensor*>(d.add_endpoint(std::make_unique<TestSensor>(1, "e")).value());
    Sink sink_a;
    DeviceManager a{kritva::core::runtime::ComponentId{101}};
    KRITVA_CHECK(a.register_device(d).has_value());
    a.set_event_sink(&sink_a);
    KRITVA_CHECK(a.initialize().has_value() && a.start().has_value());          // installs A's listener on the endpoint
    {
        DeviceManager b{kritva::core::runtime::ComponentId{102}};               // another manager that also knows the device ...
        KRITVA_CHECK(b.register_device(d).has_value());
    }                                                                           // ... is destroyed: its cleanup must not touch A's listener
    KRITVA_CHECK(e->inject_fault().has_value());
    KRITVA_CHECK(sink_a.events.size() == 1 && sink_a.events[0].source_id == a.info().id());
}

static void test_endpoint_fault_while_running() {                    // DER-607, DER-703, DER-704
    Rig r;
    r.run_up();
    KRITVA_CHECK(r.manager.health().state() == HealthState::HEALTHY && r.manager.status().code() == StatusCode::OK);

    KRITVA_CHECK(r.gyro->inject_fault("gyro lost").has_value());
    KRITVA_CHECK(r.manager.lifecycle_state() == LifecycleState::RUNNING);                                    // the manager keeps running
    KRITVA_CHECK(r.manager.health().state() == HealthState::UNHEALTHY && r.manager.health().detail() == "imu.gyro: gyro lost");
    KRITVA_CHECK(r.manager.status().code() == StatusCode::FAILED);
    KRITVA_CHECK(r.sink.events.size() == 1 && r.sink.events[0].type == EventType::ERROR
                 && r.sink.events[0].source_id == r.manager.info().id());                                    // exactly one ERROR, from the manager

    KRITVA_CHECK(r.gyro->inject_fault("again").has_value());                                                 // idempotent: no second event
    TestSample s;
    KRITVA_CHECK(!r.gyro->read(s).has_value() && r.accel->read(s).has_value());                              // the others keep working
    KRITVA_CHECK(r.sink.events.size() == 1);

    for (int round = 0; round < 5; ++round) {                                                                // no silent recovery
        KRITVA_CHECK(!r.manager.start().has_value() && !r.manager.initialize().has_value());
        KRITVA_CHECK(r.gyro->lifecycle_state() == LifecycleState::FAULT && r.manager.health().state() == HealthState::UNHEALTHY);
    }

    r.log.clear();
    KRITVA_CHECK(r.manager.stop().has_value());                                                              // healthy endpoints stop, the faulted one is skipped
    const Log stopped{"position.stop", "command.stop", "accel.stop"};
    KRITVA_CHECK(r.log == stopped);
    KRITVA_CHECK(r.gyro->lifecycle_state() == LifecycleState::FAULT && r.manager.health().state() == HealthState::UNHEALTHY);   // still visible
    KRITVA_CHECK(r.manager.shutdown().has_value());                                                          // releases the faulted endpoint
    KRITVA_CHECK(r.gyro->lifecycle_state() == LifecycleState::STOPPED && r.manager.health().state() == HealthState::UNKNOWN);

    r.run_up();                                                                                              // an explicit new lifecycle
    KRITVA_CHECK(r.manager.health().state() == HealthState::HEALTHY && r.gyro->read(s).has_value());
    KRITVA_CHECK(r.sink.events.size() == 1);
}

static void test_each_new_fault_is_one_event() {
    Rig r;
    r.run_up();
    KRITVA_CHECK(r.accel->inject_fault().has_value() && r.position->inject_fault().has_value());
    KRITVA_CHECK(r.sink.events.size() == 2);
}

static void test_no_sink_is_neutral() {
    Rig r;
    r.manager.set_event_sink(nullptr);
    r.run_up();
    KRITVA_CHECK(r.gyro->inject_fault().has_value() && r.manager.health().state() == HealthState::UNHEALTHY);
}

static void test_degraded_health_and_statistics() {                  // DER-606
    Rig r;
    r.run_up();
    r.accel->degrade("noisy");
    KRITVA_CHECK(r.manager.health().state() == HealthState::DEGRADED && r.manager.health().detail() == "imu.accel: noisy");
    TestSample s;
    KRITVA_CHECK(r.accel->read(s).has_value() && r.gyro->read(s).has_value() && r.position->read(s).has_value());
    KRITVA_CHECK(r.command->write(TestCommand{1}).has_value() && !r.command->write(TestCommand{1000}).has_value());
    const auto st = r.manager.statistics();
    KRITVA_CHECK(st.sample_count.value() == 4 && st.error_count.value() == 1);
}

static void test_capabilities_aggregate() {
    Rig r;
    // The test doubles use their endpoint id as capability id (1 and 2 in both devices); the union is by capability id.
    const auto caps = r.manager.capabilities();
    KRITVA_CHECK(caps.size() == 2 && caps.contains(kritva::core::CapabilityId{1}) && caps.contains(kritva::core::CapabilityId{2}));
}

static void test_manager_destruction_clears_listeners() {
    Device d(DeviceInfo::create(DeviceId{1}, "d").value());
    auto* e = static_cast<TestSensor*>(d.add_endpoint(std::make_unique<TestSensor>(1, "e")).value());
    Sink sink;
    {
        DeviceManager m(kritva::core::runtime::ComponentId{9});
        KRITVA_CHECK(m.register_device(d).has_value());
        m.set_event_sink(&sink);
        KRITVA_CHECK(m.initialize().has_value() && m.start().has_value());
    }                                                                                                        // the manager is gone ...
    KRITVA_CHECK(e->inject_fault().has_value() && sink.events.empty());                                      // ... a fault must not call into it
}

int main() {
    test_component_identity();
    test_deterministic_order();
    test_registry_closes_at_initialize();
    test_invalid_operations_fail_with_source();
    test_config_keys_and_scoped_settings();
    test_configuration_errors();
    test_disabled_device_is_inert();
    test_failing_hooks_fault_the_manager();
    test_failing_stop_still_stops_every_endpoint();
    test_shutdown_returns_the_first_stop_error();
    test_failing_shutdown_hook_is_reported_and_retried();
    test_failed_configure_does_not_commit_device_switches();
    test_shutdown_releases_devices_switched_off_after_the_run();
    test_devices_are_sealed_at_initialize();
    test_a_destroyed_manager_does_not_remove_anothers_listener();
    test_endpoint_fault_while_running();
    test_each_new_fault_is_one_event();
    test_no_sink_is_neutral();
    test_degraded_health_and_statistics();
    test_capabilities_aggregate();
    test_manager_destruction_clears_listeners();
    std::printf("device_manager_test: PASS\n");
    return 0;
}
