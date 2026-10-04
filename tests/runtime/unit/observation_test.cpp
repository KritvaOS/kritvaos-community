//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : observation_test.cpp
// Description : Unit tests of runtime observation: snapshot, event log, host events, diagnostics.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-OBS-001..007; RR-PERF-002; RR-SEC-001
// API         : RUNTIME-OBSERVATION-UT
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
using kritva::core::ErrorSeverity;
using kritva::core::EventType;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
using kritva::runtime::test::FixedStatistics;
using kritva::runtime::test::ProbeComponent;
using Log = std::vector<std::string>;

static void test_snapshot_states_and_order() {                       // RR-OBS-001/002/003
    RuntimeHost host;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log);
    ProbeComponent b(2, "b", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a, {b.info().id()}).has_value());
    KRITVA_CHECK(host.add_component(b).has_value());

    auto o = host.observe();
    KRITVA_CHECK(o.has_value() && o.value().state == LifecycleState::UNKNOWN && !o.value().fault);
    KRITVA_CHECK(o.value().components.size() == 2);
    KRITVA_CHECK(o.value().components[0].name == "b" && o.value().components[1].name == "a");   // dependencies first
    KRITVA_CHECK(o.value().components[0].observation.lifecycle == LifecycleState::UNKNOWN);

    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.initialize().has_value() && host.start().has_value());
    o = host.observe();
    KRITVA_CHECK(o.value().state == LifecycleState::RUNNING);
    for (const auto& c : o.value().components) {
        KRITVA_CHECK(c.observation.lifecycle == LifecycleState::RUNNING);
        KRITVA_CHECK(c.observation.health.state() == HealthState::HEALTHY);
        KRITVA_CHECK(c.observation.status.code() == core::StatusCode::OK);
    }
    KRITVA_CHECK(o.value().statistics.sample_count.value() == 6);    // 2 components x (configure, initialize, start)
    KRITVA_CHECK(o.value().statistics.error_count.value() == 0);
}

static void test_fault_observation() {                               // RR-OBS-003, RR-OBS-007
    RuntimeHost host;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log, ProbeComponent::Hook::START);
    KRITVA_CHECK(host.add_component(a).has_value());
    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.initialize().has_value());
    KRITVA_CHECK(!host.start().has_value());

    const auto o = host.observe();
    KRITVA_CHECK(o.value().state == LifecycleState::FAULT);
    KRITVA_CHECK(o.value().fault && o.value().fault->message == "probe failure");
    KRITVA_CHECK(o.value().components[0].observation.lifecycle == LifecycleState::FAULT);
    KRITVA_CHECK(o.value().components[0].observation.health.state() == HealthState::UNHEALTHY);
    KRITVA_CHECK(o.value().statistics.error_count.value() == 1);

    KRITVA_CHECK(host.runtime().reset().has_value());                // Core controlled cleanup
    KRITVA_CHECK(!host.observe().value().fault);                     // fault cleared with the state
}

static void test_component_statistics_provider() {                   // RR-OBS-004
    RuntimeHost host;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log);
    ProbeComponent b(2, "b", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a).has_value() && host.add_component(b).has_value());
    FixedStatistics stats(7, 2);
    const auto o = host.observe({{a.info().id(), &stats}}).value();
    KRITVA_CHECK(o.components[0].observation.statistics && o.components[0].observation.statistics->sample_count.value() == 7);
    KRITVA_CHECK(o.components[0].observation.statistics->error_count.value() == 2);
    KRITVA_CHECK(!o.components[1].observation.statistics);           // no provider, no statistics
}

static void test_observe_fails_on_missing_dependency() {
    RuntimeHost host;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a, {core::runtime::ComponentId{9}}).has_value());
    const auto o = host.observe();
    KRITVA_CHECK(!o.has_value() && o.error().code == ErrorCode::CONFIGURATION_ERROR);
}

static void test_event_log_bounded() {                               // RR-PERF-002
    EventLog log(3);
    for (int i = 0; i < 5; ++i) {
        core::Event e;
        e.type = EventType::STATUS;
        KRITVA_CHECK(log.report(e).has_value());
    }
    KRITVA_CHECK(log.size() == 3 && log.capacity() == 3 && log.dropped() == 2);
    const auto events = log.snapshot();
    KRITVA_CHECK(events.front().event_id.value() == 3 && events.back().event_id.value() == 5);   // oldest dropped
    log.clear();
    KRITVA_CHECK(log.size() == 0);
    KRITVA_CHECK(EventLog(0).capacity() == 1);                       // never zero-capacity
    core::Event preset;
    preset.event_id = core::Id{42};
    EventLog other;
    KRITVA_CHECK(other.report(preset).has_value() && other.snapshot()[0].event_id.value() == 42);   // keeps a given id
}

static void test_host_events() {                                     // RR-OBS-005
    RuntimeHost host;
    EventLog events;
    host.set_event_sink(&events);
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a).has_value());

    KRITVA_CHECK(host.configure(parse_configuration("runtime.name=x\n").value()).has_value());
    KRITVA_CHECK(host.run().has_value());
    auto e = events.snapshot();
    KRITVA_CHECK(e.size() == 5);                                     // configure + initialize/start/stop/shutdown
    KRITVA_CHECK(e[0].type == EventType::CONFIGURATION);
    for (std::size_t i = 1; i < e.size(); ++i) KRITVA_CHECK(e[i].type == EventType::LIFECYCLE);
    for (const auto& ev : e) KRITVA_CHECK(ev.severity == ErrorSeverity::INFO && !ev.source_id.valid());   // from the runtime
    for (std::size_t i = 0; i < e.size(); ++i) KRITVA_CHECK(e[i].event_id.value() == i + 1);               // ordered

    events.clear();
    KRITVA_CHECK(!host.start().has_value());                         // invalid transition is observable too
    e = events.snapshot();
    KRITVA_CHECK(e.size() == 1 && e[0].type == EventType::ERROR && e[0].severity == ErrorSeverity::ERROR);

    events.clear();
    KRITVA_CHECK(!host.configure(parse_configuration("x=1\n").value()).has_value());
    e = events.snapshot();
    KRITVA_CHECK(e.size() == 1 && e[0].type == EventType::CONFIGURATION && e[0].severity == ErrorSeverity::ERROR);
}

static void test_component_failure_event_source() {
    RuntimeHost host;
    EventLog events;
    host.set_event_sink(&events);
    Log log;
    ProbeComponent a(5, "a", host.runtime(), log, ProbeComponent::Hook::INITIALIZE);
    KRITVA_CHECK(host.add_component(a).has_value());
    kritva::runtime::test::configure_host(host);
    events.clear();                                                  // drop the configuration event
    KRITVA_CHECK(!host.initialize().has_value());
    const auto e = events.snapshot();
    KRITVA_CHECK(e.size() == 1 && e[0].type == EventType::ERROR && e[0].source_id == a.info().id());
}

static void test_components_report_through_core_reporter() {
    RuntimeHost host;
    EventLog events;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log);
    core::runtime::ComponentEventReporter reporter(a, events);
    core::Event e;
    e.type = EventType::HEALTH;
    e.severity = ErrorSeverity::WARNING;
    KRITVA_CHECK(reporter.report(e).has_value());
    KRITVA_CHECK(events.snapshot()[0].source_id == a.info().id());
}

static void test_no_sink_is_behavior_neutral() {
    RuntimeHost host;
    KRITVA_CHECK(host.event_sink() == nullptr);
    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.run().has_value());
}

static void test_describe() {                                        // RR-OBS-006, RR-SEC-001
    RuntimeHost host;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log, ProbeComponent::Hook::START);
    KRITVA_CHECK(host.add_component(a).has_value());
    KRITVA_CHECK(host.configure(parse_configuration("runtime.name=x\nsecret.token=hunter2\n").value()).has_value());
    KRITVA_CHECK(host.initialize().has_value() && !host.start().has_value());
    const std::string text = describe(host.observe().value());
    KRITVA_CHECK(text.find("runtime state=FAULT") != std::string::npos);
    KRITVA_CHECK(text.find("runtime fault: probe failure") != std::string::npos);
    KRITVA_CHECK(text.find("component a (id=1) state=FAULT health=UNHEALTHY") != std::string::npos);
    KRITVA_CHECK(text.find("detail=\"probe failure\"") != std::string::npos);
    KRITVA_CHECK(text.find("hunter2") == std::string::npos);         // no configuration values
}

int main() {
    test_snapshot_states_and_order();
    test_fault_observation();
    test_component_statistics_provider();
    test_observe_fails_on_missing_dependency();
    test_event_log_bounded();
    test_host_events();
    test_component_failure_event_source();
    test_components_report_through_core_reporter();
    test_no_sink_is_behavior_neutral();
    test_describe();
    std::printf("observation_test: PASS\n");
    return 0;
}
