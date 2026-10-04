//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : failure_handling_test.cpp
// Description : Unit tests of failure handling: failure report, propagation, controlled shutdown, no recovery.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-FLT-001..007; RR-DEP-004; RR-REL-004; RR-TST-005
// API         : RUNTIME-FAILURE-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <string>
#include <vector>

#include "../check.hpp"
#include "../probe_component.hpp"

using namespace kritva;
using namespace kritva::runtime;
using kritva::core::ErrorCode;
using kritva::core::EventType;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
using kritva::runtime::test::ProbeComponent;
using Log = std::vector<std::string>;
using Ids = std::vector<core::runtime::ComponentId>;

static bool has(const Log& log, const std::string& entry) {
    for (const auto& e : log) if (e == entry) return true;
    return false;
}

// c <- b <- a (a depends on b, b on c) plus independent d; all started, runtime RUNNING.
struct Fixture {
    RuntimeHost host;
    EventLog events;
    Log log;
    ProbeComponent a{1, "a", host.runtime(), log};
    ProbeComponent b{2, "b", host.runtime(), log};
    ProbeComponent c{3, "c", host.runtime(), log};
    ProbeComponent d{4, "d", host.runtime(), log};

    Fixture() {
        host.set_event_sink(&events);
        for (auto* p : {&a, &b, &c, &d}) p->set_event_sink(&events);
        KRITVA_CHECK(host.add_component(a, {b.info().id()}).has_value());
        KRITVA_CHECK(host.add_component(b, {c.info().id()}).has_value());
        KRITVA_CHECK(host.add_component(c).has_value());
        KRITVA_CHECK(host.add_component(d).has_value());
        KRITVA_CHECK(host.initialize().has_value() && host.start().has_value());
        events.clear();
    }
};

static void test_no_failure_report_when_healthy() {
    Fixture f;
    const auto r = f.host.failure_report();
    KRITVA_CHECK(r.has_value() && !r.value().any() && r.value().affected.empty());
}

static void test_injected_failure_is_observable() {                  // RR-FLT-001..004
    Fixture f;
    f.c.inject_failure();

    const auto o = f.host.observe().value();
    for (const auto& rec : o.components) {
        if (rec.name == "c") {
            KRITVA_CHECK(rec.observation.lifecycle == LifecycleState::FAULT);
            KRITVA_CHECK(rec.observation.health.state() == HealthState::UNHEALTHY);
            KRITVA_CHECK(rec.observation.status.code() == core::StatusCode::FAILED);
        } else {
            KRITVA_CHECK(rec.observation.lifecycle == LifecycleState::RUNNING);   // others untouched
        }
    }
    const auto events = f.events.snapshot();
    KRITVA_CHECK(events.size() == 1 && events[0].type == EventType::ERROR && events[0].source_id == f.c.info().id());
    KRITVA_CHECK(f.host.state() == LifecycleState::RUNNING);         // Core is not told; the host observes it
}

static void test_failure_propagation_report() {                      // RR-DEP-004
    Fixture f;
    f.c.inject_failure();
    auto r = f.host.failure_report().value();
    KRITVA_CHECK((r.failed == Ids{f.c.info().id()}));
    KRITVA_CHECK((r.affected == Ids{f.b.info().id(), f.a.info().id()}));   // transitive dependents, d unaffected

    Fixture g;
    g.a.inject_failure();                                            // nothing depends on a
    r = g.host.failure_report().value();
    KRITVA_CHECK((r.failed == Ids{g.a.info().id()}) && r.affected.empty());
}

static void test_report_is_deterministic_and_pure() {                // RR-FLT-005
    Fixture f;
    f.c.inject_failure();
    const auto first = f.host.failure_report().value();
    const auto second = f.host.failure_report().value();
    KRITVA_CHECK(first.failed == second.failed && first.affected == second.affected);
    KRITVA_CHECK(f.c.lifecycle_state() == LifecycleState::FAULT);    // reporting changed nothing
    KRITVA_CHECK(f.b.lifecycle_state() == LifecycleState::RUNNING);
}

static void test_controlled_shutdown_after_failure() {               // RR-FLT-006
    Fixture f;
    f.c.inject_failure();
    f.log.clear();
    f.events.clear();
    KRITVA_CHECK(f.host.controlled_shutdown().has_value());

    KRITVA_CHECK(f.host.state() == LifecycleState::STOPPED);
    for (auto* p : {&f.a, &f.b, &f.c, &f.d}) KRITVA_CHECK(p->lifecycle_state() == LifecycleState::STOPPED);
    for (const char* name : {"a", "b", "c", "d"}) {
        KRITVA_CHECK(has(f.log, std::string(name) + ".shutdown:FAULT") || has(f.log, std::string(name) + ".shutdown:STOPPED"));
    }
    KRITVA_CHECK(f.host.runtime().fault_error() == nullptr);
    // The failure of the stop sequence is itself observable.
    bool saw_error = false;
    for (const auto& e : f.events.snapshot()) if (e.type == EventType::ERROR) saw_error = true;
    KRITVA_CHECK(saw_error);
    KRITVA_CHECK(!f.host.failure_report().value().any());            // nothing left failed
}

static void test_no_silent_recovery() {                              // RR-FLT-007
    Fixture f;
    f.c.inject_failure();
    auto r = f.host.start();                                         // already RUNNING: not a restart path
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_STATE);
    KRITVA_CHECK(f.c.lifecycle_state() == LifecycleState::FAULT);    // stays FAULT until shutdown
    KRITVA_CHECK(f.c.health().state() == HealthState::UNHEALTHY);
    KRITVA_CHECK(f.host.failure_report().value().any());
}

static void test_failure_in_each_step_then_controlled_shutdown() {   // RR-REL-004
    for (auto hook : {ProbeComponent::Hook::INITIALIZE, ProbeComponent::Hook::START, ProbeComponent::Hook::STOP}) {
        RuntimeHost host;
        Log log;
        ProbeComponent a(1, "a", host.runtime(), log, hook);
        ProbeComponent b(2, "b", host.runtime(), log);
        KRITVA_CHECK(host.add_component(a, {b.info().id()}).has_value() && host.add_component(b).has_value());
        auto r = host.initialize();
        if (r.has_value()) r = host.start();
        if (r.has_value()) r = host.stop();
        KRITVA_CHECK(!r.has_value());
        KRITVA_CHECK(host.state() == LifecycleState::FAULT);
        KRITVA_CHECK(host.controlled_shutdown().has_value());
        KRITVA_CHECK(host.state() == LifecycleState::STOPPED);
        KRITVA_CHECK(a.lifecycle_state() == LifecycleState::STOPPED && b.lifecycle_state() == LifecycleState::STOPPED);
    }
}

static void test_controlled_shutdown_from_any_quiet_state() {
    RuntimeHost host;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a).has_value());
    KRITVA_CHECK(host.controlled_shutdown().has_value());            // UNKNOWN
    KRITVA_CHECK(host.initialize().has_value());
    KRITVA_CHECK(host.controlled_shutdown().has_value());            // READY
    KRITVA_CHECK(host.state() == LifecycleState::STOPPED);
    KRITVA_CHECK(host.controlled_shutdown().has_value());            // STOPPED, idempotent
    KRITVA_CHECK(host.state() == LifecycleState::STOPPED);
}

static void test_restart_after_failure_is_a_new_explicit_lifecycle() {   // RR-REL-003
    Fixture f;
    f.c.inject_failure();
    KRITVA_CHECK(f.host.controlled_shutdown().has_value());
    KRITVA_CHECK(f.host.initialize().has_value() && f.host.start().has_value());   // explicit operator action
    KRITVA_CHECK(!f.host.failure_report().value().any());
    KRITVA_CHECK(f.host.controlled_shutdown().has_value());
}

static void test_scenario_is_deterministic() {                       // RR-FLT-005
    auto scenario = [] {
        Fixture f;
        f.c.inject_failure();
        (void)f.host.controlled_shutdown();
        std::vector<int> types;
        for (const auto& e : f.events.snapshot()) types.push_back(static_cast<int>(e.type));
        return std::pair{f.log, types};
    };
    KRITVA_CHECK(scenario() == scenario());
}

int main() {
    test_no_failure_report_when_healthy();
    test_injected_failure_is_observable();
    test_failure_propagation_report();
    test_report_is_deterministic_and_pure();
    test_controlled_shutdown_after_failure();
    test_no_silent_recovery();
    test_failure_in_each_step_then_controlled_shutdown();
    test_controlled_shutdown_from_any_quiet_state();
    test_restart_after_failure_is_a_new_explicit_lifecycle();
    test_scenario_is_deterministic();
    std::printf("failure_handling_test: PASS\n");
    return 0;
}
