//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : mock_test.cpp
// Description : Unit and conformance tests of the mock IMU and motor devices.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-702; DER-703; DER-802; DER-803; DER-502; DER-501
// API         : HARDWARE-MOCK-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "../../runtime/check.hpp"
#include "../conformance/endpoint_conformance.hpp"

#include <kritva/hardware/mock/mock_imu.hpp>
#include <kritva/hardware/mock/mock_motor.hpp>

using namespace kritva::hardware;
using namespace kritva::hardware::mock;
using kritva::core::ErrorCode;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
namespace conf = kritva::hardware::conformance;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

static kritva::core::Configuration cfg(std::initializer_list<std::pair<const char*, std::int64_t>> values) {
    kritva::core::Configuration c;
    for (const auto& [k, v] : values) KRITVA_CHECK(c.set(kritva::core::Parameter{k, v, {}}).has_value());
    return c;
}

static void up(Endpoint& e) { KRITVA_CHECK(e.initialize().has_value() && e.start().has_value()); }

// A position endpoint together with the parts it refers to, so that it can stand alone in a fixture.
struct MotorParts {
    std::shared_ptr<MotorModel> model = std::make_shared<MotorModel>();
    MockMotorCommandEndpoint command{model};
};
struct OwnedPosition : private MotorParts, public MockPositionEndpoint {
    OwnedPosition() : MotorParts(), MockPositionEndpoint(MotorParts::model, MotorParts::command) {}
};

static void expect_conforms(const std::string& v) {
    if (!v.empty()) { std::fprintf(stderr, "conformance violation: %s\n", v.c_str()); std::exit(1); }
}

static void test_conformance() {                                     // DER-802: mocks use the same contracts
    conf::Fixture<AccelerationEndpoint> a{[] { return std::make_unique<MockAccelerationEndpoint>(); },
                                          [](AccelerationEndpoint& e) { return static_cast<MockAccelerationEndpoint&>(e).inject_fault(); }};
    expect_conforms(conf::check_sensor_contract<AccelerationSample>(a));
    conf::Fixture<AngularVelocityEndpoint> g{[] { return std::make_unique<MockAngularVelocityEndpoint>(); },
                                             [](AngularVelocityEndpoint& e) { return static_cast<MockAngularVelocityEndpoint&>(e).inject_fault(); }};
    expect_conforms(conf::check_sensor_contract<AngularVelocitySample>(g));
    conf::Fixture<PositionEndpoint> p{[] { return std::make_unique<OwnedPosition>(); },
                                      [](PositionEndpoint& e) { return static_cast<OwnedPosition&>(e).inject_fault(); }};
    expect_conforms(conf::check_sensor_contract<PositionSample>(p));
    conf::Fixture<MotorCommandEndpoint> m{[] { return std::make_unique<MockMotorCommandEndpoint>(std::make_shared<MotorModel>()); },
                                          [](MotorCommandEndpoint& e) { return static_cast<MockMotorCommandEndpoint&>(e).inject_fault(); }};
    expect_conforms(conf::check_actuator_contract<MotorCommand>(m));
}

static void test_nominal_acceleration_and_gyro() {                   // deterministic nominal data
    MockAccelerationEndpoint a;
    up(a);
    for (std::uint64_t n = 1; n <= 4; ++n) {
        AccelerationSample s;
        KRITVA_CHECK(a.read(s).has_value());
        KRITVA_CHECK(s.sequence == n && s.value.x == static_cast<double>(n - 1) && s.value.y == -static_cast<double>(n - 1) && s.value.z == 9.81);
        KRITVA_CHECK(s.timestamp == virtual_timestamp(n) && is_valid(s));
    }
    MockAngularVelocityEndpoint g;
    up(g);
    AngularVelocitySample w;
    KRITVA_CHECK(g.read(w).has_value() && g.read(w).has_value() && w.sequence == 2 && w.value.x == 1.0 && w.value.y == 0.0 && w.value.z == 0.0);
}

static void test_identical_instances_give_identical_data() {
    auto run = [] {
        MockAccelerationEndpoint a;
        KRITVA_CHECK(a.configure(cfg({{"start", 10}, {"step", -3}})).has_value());
        up(a);
        std::vector<double> xs;
        for (int i = 0; i < 5; ++i) { AccelerationSample s; KRITVA_CHECK(a.read(s).has_value()); xs.push_back(s.value.x); }
        return xs;
    };
    const std::vector<double> expected{10, 7, 4, 1, -2};
    KRITVA_CHECK(run() == expected && run() == run());
}

static void test_sequence_restarts_with_each_live_period() {
    MockAccelerationEndpoint a;
    AccelerationSample s;
    up(a);
    KRITVA_CHECK(a.read(s).has_value() && a.read(s).has_value() && s.sequence == 2);
    KRITVA_CHECK(a.stop().has_value() && a.shutdown().has_value());
    up(a);
    KRITVA_CHECK(a.read(s).has_value() && s.sequence == 1 && s.timestamp == virtual_timestamp(1));
}

static void test_settings_and_configuration_errors() {
    MockAccelerationEndpoint a;
    const std::vector<std::string> expected{"start", "step", "fail_after_ops", "fault_after_ops"};
    KRITVA_CHECK(a.setting_names() == expected);
    for (const char* name : {"start", "step", "fail_after_ops", "fault_after_ops"}) {
        for (std::int64_t bad : {std::int64_t{2'000'000}, std::int64_t{-2'000'000}, std::int64_t{-1}}) {
            MockAccelerationEndpoint e;
            const auto r = e.configure(cfg({{name, bad}}));
            const bool is_count = std::string(name).rfind("fail", 0) == 0 || std::string(name).rfind("fault", 0) == 0;
            if (is_count || bad == 2'000'000 || bad == -2'000'000) KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::CONFIGURATION_ERROR);
        }
    }
    kritva::core::Configuration wrong_type;
    KRITVA_CHECK(wrong_type.set(kritva::core::Parameter{"start", std::string("x"), {}}).has_value());
    MockAccelerationEndpoint e;
    KRITVA_CHECK(!e.configure(wrong_type).has_value() && e.lifecycle_state() == LifecycleState::UNKNOWN);
    // A rejected configure changes nothing: the previous settings stay in force.
    KRITVA_CHECK(e.configure(cfg({{"start", 5}})).has_value());
    KRITVA_CHECK(!e.configure(cfg({{"start", 9}, {"step", 2'000'000}})).has_value());
    up(e);
    AccelerationSample s;
    KRITVA_CHECK(e.read(s).has_value() && s.value.x == 5.0);
}

static void test_read_failure_injection() {                          // DER-803: read failure
    MockAccelerationEndpoint a;
    KRITVA_CHECK(a.configure(cfg({{"fail_after_ops", 2}})).has_value());
    up(a);
    AccelerationSample s;
    KRITVA_CHECK(a.read(s).has_value() && a.read(s).has_value());
    const auto r = a.read(s);                                                      // the third read fails, once
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INTERNAL_ERROR && r.error().message == "injected read failure");
    KRITVA_CHECK(a.lifecycle_state() == LifecycleState::RUNNING && a.health().state() == HealthState::HEALTHY);   // not a fault
    KRITVA_CHECK(a.read(s).has_value() && s.sequence == 3);                        // and it carries on, without a gap in the data
    KRITVA_CHECK(a.read(s).has_value() && s.sequence == 4);                        // only once
    KRITVA_CHECK(a.statistics().sample_count.value() == 4 && a.statistics().error_count.value() == 1);
    KRITVA_CHECK(a.last_error() && a.last_error()->message == "injected read failure");

    MockAngularVelocityEndpoint g;
    up(g);
    g.fail_next_operation();                                                       // one-shot, programmatic
    AngularVelocitySample w;
    KRITVA_CHECK(!g.read(w).has_value() && g.read(w).has_value() && w.sequence == 1);
}

static void test_fault_injection() {                                 // DER-803: fault, no silent recovery
    MockAccelerationEndpoint a;
    KRITVA_CHECK(a.configure(cfg({{"fault_after_ops", 3}})).has_value());
    int faults = 0;
    a.set_fault_listener(&faults, [&](const Endpoint&) { ++faults; });
    up(a);
    AccelerationSample s;
    for (int i = 0; i < 3; ++i) KRITVA_CHECK(a.read(s).has_value());               // the third still succeeds ...
    KRITVA_CHECK(a.lifecycle_state() == LifecycleState::FAULT && faults == 1);     // ... and then the endpoint is faulted
    KRITVA_CHECK(a.health().state() == HealthState::UNHEALTHY && a.status().code() == kritva::core::StatusCode::FAILED);
    for (int i = 0; i < 4; ++i) {
        const auto r = a.read(s);
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::RESOURCE_UNAVAILABLE && a.lifecycle_state() == LifecycleState::FAULT);
    }
    KRITVA_CHECK(faults == 1 && s.sequence == 3);
    KRITVA_CHECK(a.shutdown().has_value() && a.initialize().has_value() && a.start().has_value());   // explicit restart, new period
    KRITVA_CHECK(a.read(s).has_value() && s.sequence == 1 && a.lifecycle_state() == LifecycleState::RUNNING);
}

static void test_degradation() {                                     // DER-803: health degradation
    MockAngularVelocityEndpoint g;
    up(g);
    g.degrade("noisy");
    KRITVA_CHECK(g.health().state() == HealthState::DEGRADED && g.health().detail() == "noisy");
    AngularVelocitySample w;
    KRITVA_CHECK(g.read(w).has_value());                                           // degraded endpoints still work
    g.clear_degradation();
    KRITVA_CHECK(g.health().state() == HealthState::HEALTHY);
}

static void test_motor_nominal_and_validation() {                    // DER-502
    auto model = std::make_shared<MotorModel>();
    MockMotorCommandEndpoint m(model);
    KRITVA_CHECK(m.limits().min_rad_s == -1.0 && m.limits().max_rad_s == 1.0);
    KRITVA_CHECK(m.configure(cfg({{"min_rad_s", -5}, {"max_rad_s", 10}})).has_value());
    up(m);
    KRITVA_CHECK(m.write(MotorCommand{2.5}).has_value() && model->velocity_rad_s == 2.5 && m.effective_velocity() == 2.5);
    KRITVA_CHECK(m.write(MotorCommand{10.0}).has_value() && m.write(MotorCommand{-5.0}).has_value());   // inclusive
    for (double bad : {10.5, -5.5, kNaN, std::numeric_limits<double>::infinity()}) {
        const auto r = m.write(MotorCommand{bad});
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
        KRITVA_CHECK(model->velocity_rad_s == -5.0);                                // rejected: nothing applied
    }
    KRITVA_CHECK(m.lifecycle_state() == LifecycleState::RUNNING);
    KRITVA_CHECK(m.statistics().sample_count.value() == 3 && m.statistics().error_count.value() == 4);
}

static void test_motor_limit_configuration() {
    auto model = std::make_shared<MotorModel>();
    MockMotorCommandEndpoint m(model);
    auto r = m.configure(cfg({{"min_rad_s", 3}, {"max_rad_s", 2}}));
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::CONFIGURATION_ERROR);
    KRITVA_CHECK(!m.configure(cfg({{"max_rad_s", 5000}})).has_value());
    KRITVA_CHECK(m.limits().min_rad_s == -1.0 && m.limits().max_rad_s == 1.0);     // unchanged by the rejected attempts
    KRITVA_CHECK(m.set_limits(MotorLimits{-0.5, 0.25}).has_value() && m.limits().max_rad_s == 0.25);   // fractional limits
    KRITVA_CHECK(!m.set_limits(MotorLimits{1.0, -1.0}).has_value() && !m.set_limits(MotorLimits{kNaN, 1.0}).has_value());
    KRITVA_CHECK(m.limits().min_rad_s == -0.5);
}

static void test_fractional_limits_survive_configuration() {          // m1: no lossy integer round trip
    auto model = std::make_shared<MotorModel>();
    MockMotorCommandEndpoint m(model);
    KRITVA_CHECK(m.set_limits(MotorLimits{-0.5, 0.25}).has_value());
    KRITVA_CHECK(m.configure(cfg({})).has_value());                                // no limit settings: the limits stay exactly as they are
    KRITVA_CHECK(m.limits().min_rad_s == -0.5 && m.limits().max_rad_s == 0.25);
    KRITVA_CHECK(m.configure(cfg({{"max_rad_s", 7}})).has_value());                // only the setting given changes
    KRITVA_CHECK(m.limits().min_rad_s == -0.5 && m.limits().max_rad_s == 7.0);
    KRITVA_CHECK(m.configure(cfg({{"fail_after_writes", 3}})).has_value());        // unrelated settings leave the limits alone
    KRITVA_CHECK(m.limits().min_rad_s == -0.5 && m.limits().max_rad_s == 7.0);
}

static void test_set_limits_ceiling_and_lifecycle() {                  // m1
    auto model = std::make_shared<MotorModel>();
    MockMotorCommandEndpoint m(model);
    for (const MotorLimits& bad : {MotorLimits{-1e300, 1e300}, MotorLimits{-1000.5, 1}, MotorLimits{-1, 1000.5}}) {
        const auto r = m.set_limits(bad);
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
    }
    KRITVA_CHECK(m.set_limits(MotorLimits{-1000.0, 1000.0}).has_value());          // the ceiling itself is allowed
    KRITVA_CHECK(m.configure(cfg({})).has_value() && m.limits().min_rad_s == -1000.0);   // and survives configuration (no UB cast)
    KRITVA_CHECK(m.initialize().has_value());
    for (int phase = 0; phase < 3; ++phase) {                                      // READY, RUNNING, FAULT: limits are frozen
        const auto r = m.set_limits(MotorLimits{-1.0, 1.0});
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_STATE && m.limits().max_rad_s == 1000.0);
        if (phase == 0) KRITVA_CHECK(m.start().has_value());
        if (phase == 1) KRITVA_CHECK(m.inject_fault().has_value());
    }
    KRITVA_CHECK(m.shutdown().has_value() && m.set_limits(MotorLimits{-2.0, 2.0}).has_value());   // allowed again once stopped
}

static void test_faulted_motor_model_is_zero() {                       // m2: the model itself, not only the accessor
    auto model = std::make_shared<MotorModel>();
    MockMotorCommandEndpoint m(model);
    KRITVA_CHECK(m.configure(cfg({{"max_rad_s", 1000}})).has_value());
    up(m);
    KRITVA_CHECK(m.write(MotorCommand{999.0}).has_value() && model->velocity_rad_s == 999.0);
    KRITVA_CHECK(m.inject_fault().has_value());
    KRITVA_CHECK(model->velocity_rad_s == 0.0 && m.effective_velocity() == 0.0);
    // The same through a scheduled fault and through a failing hook.
    auto model2 = std::make_shared<MotorModel>();
    MockMotorCommandEndpoint s(model2);
    KRITVA_CHECK(s.configure(cfg({{"fault_after_writes", 1}})).has_value());
    up(s);
    KRITVA_CHECK(s.write(MotorCommand{0.75}).has_value() && s.lifecycle_state() == LifecycleState::FAULT && model2->velocity_rad_s == 0.0);
    // And through the device: the paired position endpoint stops moving, the model reads zero.
    MockMotorDevice dev(DeviceInfo::create(DeviceId{2}, "motor").value());
    up(dev.command()); up(dev.position());
    KRITVA_CHECK(dev.command().write(MotorCommand{1.0}).has_value() && dev.model().velocity_rad_s == 1.0);
    KRITVA_CHECK(dev.command().inject_fault().has_value() && dev.model().velocity_rad_s == 0.0);
}

static void test_motor_write_failure_and_fault() {                   // DER-803: write failure, fault
    auto model = std::make_shared<MotorModel>();
    MockMotorCommandEndpoint m(model);
    KRITVA_CHECK(m.configure(cfg({{"fail_after_writes", 1}})).has_value());
    up(m);
    KRITVA_CHECK(m.write(MotorCommand{0.5}).has_value());
    const auto r = m.write(MotorCommand{0.9});                                      // fails once, not applied
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INTERNAL_ERROR && r.error().message == "injected write failure");
    KRITVA_CHECK(model->velocity_rad_s == 0.5 && m.lifecycle_state() == LifecycleState::RUNNING);
    KRITVA_CHECK(m.write(MotorCommand{0.9}).has_value() && model->velocity_rad_s == 0.9);

    auto model2 = std::make_shared<MotorModel>();
    MockMotorCommandEndpoint f(model2);
    KRITVA_CHECK(f.configure(cfg({{"fault_after_writes", 2}})).has_value());
    up(f);
    KRITVA_CHECK(f.write(MotorCommand{0.4}).has_value() && f.write(MotorCommand{0.6}).has_value());
    KRITVA_CHECK(f.lifecycle_state() == LifecycleState::FAULT && f.effective_velocity() == 0.0);   // fail-safe: a faulted motor does not move
    KRITVA_CHECK(f.write(MotorCommand{0.1}).error().code == ErrorCode::RESOURCE_UNAVAILABLE);
}

static void test_motor_is_fail_safe() {                              // safety
    auto model = std::make_shared<MotorModel>();
    MockMotorCommandEndpoint m(model);
    KRITVA_CHECK(m.initialize().has_value() && model->velocity_rad_s == 0.0);
    KRITVA_CHECK(m.start().has_value() && m.write(MotorCommand{1.0}).has_value() && m.effective_velocity() == 1.0);
    KRITVA_CHECK(m.stop().has_value());
    KRITVA_CHECK(model->velocity_rad_s == 0.0 && m.effective_velocity() == 0.0);                // stopping stops the joint
    KRITVA_CHECK(m.initialize().has_value() && m.start().has_value() && model->velocity_rad_s == 0.0);   // a restart never begins moving
    KRITVA_CHECK(m.write(MotorCommand{1.0}).has_value() && m.inject_fault().has_value());
    KRITVA_CHECK(m.effective_velocity() == 0.0);                                                // faulted: effectively stopped
    KRITVA_CHECK(m.shutdown().has_value() && model->velocity_rad_s == 0.0);
}

static void test_position_follows_command() {
    MockMotorDevice motor(DeviceInfo::create(DeviceId{2}, "shoulder_motor").value());
    auto& command = motor.command();
    auto& position = motor.position();
    KRITVA_CHECK(position.configure(cfg({{"initial", 3}})).has_value());
    up(command);
    up(position);
    PositionSample p;
    KRITVA_CHECK(position.read(p).has_value() && p.value == 3.0 && p.sequence == 1);               // no command: not moving
    KRITVA_CHECK(command.write(MotorCommand{1.0}).has_value());
    for (int n = 2; n <= 4; ++n) {
        KRITVA_CHECK(position.read(p).has_value() && p.sequence == static_cast<std::uint64_t>(n));
        KRITVA_CHECK(std::fabs(p.value - (3.0 + (n - 1) * 0.01)) < 1e-12 && p.timestamp == virtual_timestamp(n));
    }
    const double at = p.value;
    KRITVA_CHECK(command.inject_fault().has_value());                                              // the motor faults ...
    KRITVA_CHECK(position.read(p).has_value() && p.value == at);                                   // ... and the joint stops
    // The same sequence repeats exactly in a new device.
    MockMotorDevice other(DeviceInfo::create(DeviceId{9}, "other_motor").value());
    KRITVA_CHECK(other.position().configure(cfg({{"initial", 3}})).has_value());
    up(other.command()); up(other.position());
    PositionSample q;
    KRITVA_CHECK(other.position().read(q).has_value() && q.value == 3.0);
    KRITVA_CHECK(other.command().write(MotorCommand{1.0}).has_value() && other.position().read(q).has_value() && q.value == 3.01);
}

static void test_devices_and_capabilities() {                        // DER-802
    MockImuDevice imu(DeviceInfo::create(DeviceId{1}, "left_arm_imu").value());
    MockMotorDevice motor(DeviceInfo::create(DeviceId{2}, "shoulder_motor").value());
    KRITVA_CHECK(imu.endpoints().size() == 2 && imu.find_endpoint("acceleration") == &imu.acceleration() && imu.find_endpoint(EndpointId{2}) == &imu.angular_velocity());
    KRITVA_CHECK(motor.endpoints().size() == 2 && motor.find_endpoint("command") == &motor.command() && motor.find_endpoint("position") == &motor.position());
    KRITVA_CHECK(imu.capabilities().contains(kCapabilityAcceleration) && imu.capabilities().contains(kCapabilityAngularVelocity));
    KRITVA_CHECK(motor.capabilities().contains(kCapabilityMotorCommand) && motor.capabilities().contains(kCapabilityPosition));
    KRITVA_CHECK(imu.acceleration().info().direction() == EndpointDirection::SENSOR && motor.command().info().direction() == EndpointDirection::ACTUATOR);
    KRITVA_CHECK(imu.health().state() == HealthState::UNKNOWN);
    for (Endpoint* e : imu.endpoints()) up(*e);
    KRITVA_CHECK(imu.health().state() == HealthState::HEALTHY);
    imu.angular_velocity().degrade("drift");
    KRITVA_CHECK(imu.health().state() == HealthState::DEGRADED && imu.health().detail() == "angular_velocity: drift");
    KRITVA_CHECK(imu.angular_velocity().inject_fault().has_value() && imu.health().state() == HealthState::UNHEALTHY);
}

int main() {
    test_conformance();
    test_nominal_acceleration_and_gyro();
    test_identical_instances_give_identical_data();
    test_sequence_restarts_with_each_live_period();
    test_settings_and_configuration_errors();
    test_read_failure_injection();
    test_fault_injection();
    test_degradation();
    test_motor_nominal_and_validation();
    test_motor_limit_configuration();
    test_fractional_limits_survive_configuration();
    test_set_limits_ceiling_and_lifecycle();
    test_faulted_motor_model_is_zero();
    test_motor_write_failure_and_fault();
    test_motor_is_fail_safe();
    test_position_follows_command();
    test_devices_and_capabilities();
    std::printf("mock_test: PASS\n");
    return 0;
}
