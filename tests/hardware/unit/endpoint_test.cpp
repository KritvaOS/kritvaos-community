//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : endpoint_test.cpp
// Description : Unit and conformance tests of the Endpoint, SensorEndpoint and ActuatorEndpoint contracts.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-301..307; DER-401..404; DER-407; DER-501..505; DER-604; DER-606; DER-607; DER-703
// API         : HARDWARE-ENDPOINT-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <string>

#include "../../runtime/check.hpp"
#include "../conformance/endpoint_conformance.hpp"
#include "../support/test_endpoints.hpp"

using namespace kritva::hardware;
using namespace kritva::hardware::test;
using kritva::core::ErrorCode;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
namespace conf = kritva::hardware::conformance;

static void expect_conforms(const std::string& violation) {
    if (!violation.empty()) { std::fprintf(stderr, "conformance violation: %s\n", violation.c_str()); std::exit(1); }
}

static void test_conformance_of_the_test_doubles() {
    conf::Fixture<Endpoint> plain{[] { return std::make_unique<TestEndpoint>(); },
                                  [](Endpoint& e) { return static_cast<TestEndpoint&>(e).inject_fault(); }};
    expect_conforms(conf::check_endpoint_contract(plain, EndpointDirection::SENSOR));

    conf::Fixture<SensorEndpoint<TestSample>> sensor{[] { return std::make_unique<TestSensor>(); },
                                                     [](SensorEndpoint<TestSample>& e) { return static_cast<TestSensor&>(e).inject_fault(); }};
    expect_conforms(conf::check_sensor_contract(sensor));

    conf::Fixture<ActuatorEndpoint<TestCommand>> actuator{[] { return std::make_unique<TestActuator>(); },
                                                          [](ActuatorEndpoint<TestCommand>& e) { return static_cast<TestActuator&>(e).inject_fault(); }};
    expect_conforms(conf::check_actuator_contract(actuator));
}

static void test_conformance_detects_a_broken_endpoint() {            // the suite can fail
    // A fixture whose fault injection does nothing cannot satisfy "a faulted endpoint is FAULT".
    conf::Fixture<Endpoint> lazy{[] { return std::make_unique<TestEndpoint>(); },
                                 [](Endpoint&) { return kritva::core::Result<void>::success(); }};
    KRITVA_CHECK(!conf::check_endpoint_contract(lazy, EndpointDirection::SENSOR).empty());
    // Wrong direction is reported.
    conf::Fixture<Endpoint> plain{[] { return std::make_unique<TestEndpoint>(); },
                                  [](Endpoint& e) { return static_cast<TestEndpoint&>(e).inject_fault(); }};
    KRITVA_CHECK(!conf::check_endpoint_contract(plain, EndpointDirection::ACTUATOR).empty());
}

static void test_failing_hooks_fault_the_endpoint() {                 // DER-306, DER-701
    for (auto at : {FailAt::INITIALIZE, FailAt::START, FailAt::STOP}) {
        TestEndpoint e;
        e.fail_at = at;
        kritva::core::Result<void> r = e.initialize();
        if (r) r = e.start();
        if (r) r = e.stop();
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INTERNAL_ERROR && r.error().message == "hook failure");   // returned unchanged
        KRITVA_CHECK(e.lifecycle_state() == LifecycleState::FAULT);
        KRITVA_CHECK(e.health().state() == HealthState::UNHEALTHY && e.health().detail() == "hook failure");
        KRITVA_CHECK(e.last_error() && e.last_error()->message == "hook failure");
        e.fail_at = FailAt::NONE;
        KRITVA_CHECK(e.shutdown().has_value() && e.lifecycle_state() == LifecycleState::STOPPED);
    }
}

static void test_failing_configure_and_shutdown_leave_the_state() {
    TestEndpoint e;
    e.fail_at = FailAt::CONFIGURE;
    KRITVA_CHECK(!e.configure(kritva::core::Configuration{}).has_value() && e.lifecycle_state() == LifecycleState::UNKNOWN);
    e.fail_at = FailAt::NONE;
    KRITVA_CHECK(e.initialize().has_value() && e.start().has_value() && e.stop().has_value());
    e.fail_at = FailAt::SHUTDOWN;
    KRITVA_CHECK(!e.shutdown().has_value() && e.lifecycle_state() == LifecycleState::STOPPED);   // state unchanged
    e.fail_at = FailAt::NONE;
    KRITVA_CHECK(e.shutdown().has_value() && e.shutdown_calls == 2);                              // a retry resumes
    KRITVA_CHECK(e.shutdown().has_value() && e.shutdown_calls == 2);                              // then it is a no-op
}

static void test_shutdown_releases_each_live_period_once() {
    TestEndpoint e;
    KRITVA_CHECK(e.shutdown().has_value() && e.shutdown_calls == 0);                              // never live
    for (int period = 1; period <= 3; ++period) {
        KRITVA_CHECK(e.initialize().has_value() && e.start().has_value() && e.stop().has_value());
        KRITVA_CHECK(e.shutdown().has_value() && e.shutdown_calls == period);
    }
    KRITVA_CHECK(e.initialize_calls == 3 && e.start_calls == 3 && e.stop_calls == 3);
}

static void test_degraded_health() {
    TestEndpoint e;
    e.degrade("noisy");
    KRITVA_CHECK(e.health().state() == HealthState::UNKNOWN);                                     // only while RUNNING
    KRITVA_CHECK(e.initialize().has_value() && e.start().has_value());
    e.degrade("noisy");
    KRITVA_CHECK(e.health().state() == HealthState::DEGRADED && e.health().detail() == "noisy");
    e.recover_degradation();
    KRITVA_CHECK(e.health().state() == HealthState::HEALTHY);
    e.degrade("again");
    KRITVA_CHECK(e.stop().has_value());
    KRITVA_CHECK(e.initialize().has_value() && e.start().has_value() && e.health().state() == HealthState::HEALTHY);   // cleared by a new period
}

static void test_typed_sensor_operations() {                          // DER-501, DER-503
    TestSensor s;
    TestSample out{-1};
    KRITVA_CHECK(!s.read(out).has_value() && out.value == -1);        // not running: untouched
    KRITVA_CHECK(s.reads == 0);                                       // the implementation was not called
    KRITVA_CHECK(s.initialize().has_value() && s.start().has_value());
    KRITVA_CHECK(s.read(out).has_value() && out.value == 1 && s.read(out).has_value() && out.value == 2);   // deterministic
    s.fail_next_read = true;
    const auto r = s.read(out);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INTERNAL_ERROR);
    KRITVA_CHECK(s.lifecycle_state() == LifecycleState::RUNNING);     // a failed read does not by itself fault the endpoint
    KRITVA_CHECK(s.statistics().sample_count.value() == 2);
    KRITVA_CHECK(s.statistics().error_count.value() == 2);            // the refused read and the failed read
    KRITVA_CHECK(s.last_error() && s.last_error()->message == "read failure");
    KRITVA_CHECK(s.read(out).has_value() && out.value == 4);          // and the endpoint keeps working
}

static void test_typed_actuator_operations() {                        // DER-502, DER-504
    TestActuator a;
    KRITVA_CHECK(!a.write(TestCommand{5}).has_value() && !a.applied);                              // not running: no effect
    KRITVA_CHECK(a.initialize().has_value() && a.start().has_value());
    KRITVA_CHECK(a.write(TestCommand{5}).has_value() && a.applied && a.applied->value == 5);
    const auto r = a.write(TestCommand{1000});                        // invalid: rejected, not applied
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(a.applied->value == 5);
    KRITVA_CHECK(a.inject_fault().has_value());
    const auto f = a.write(TestCommand{6});
    KRITVA_CHECK(!f.has_value() && f.error().code == ErrorCode::RESOURCE_UNAVAILABLE && a.applied->value == 5);
}

static void test_capabilities_are_stable() {                          // DER-303, DER-605
    TestSensor s(9, "accel");
    KRITVA_CHECK(s.capabilities().size() == 1 && s.capabilities().contains(kritva::core::CapabilityId{9}));
    KRITVA_CHECK(s.initialize().has_value() && s.start().has_value() && s.inject_fault().has_value());
    KRITVA_CHECK(s.capabilities().size() == 1);
}

int main() {
    test_conformance_of_the_test_doubles();
    test_conformance_detects_a_broken_endpoint();
    test_failing_hooks_fault_the_endpoint();
    test_failing_configure_and_shutdown_leave_the_state();
    test_shutdown_releases_each_live_period_once();
    test_degraded_health();
    test_typed_sensor_operations();
    test_typed_actuator_operations();
    test_capabilities_are_stable();
    std::printf("endpoint_test: PASS\n");
    return 0;
}
