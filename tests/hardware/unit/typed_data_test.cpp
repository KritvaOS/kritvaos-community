//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : typed_data_test.cpp
// Description : Unit and conformance tests of the typed samples, MotorCommand validation and typed endpoint interfaces.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-501..507; DER-702
// API         : HARDWARE-TYPED-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <cmath>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "../../runtime/check.hpp"
#include "../conformance/endpoint_conformance.hpp"
#include "../support/test_endpoints.hpp"

#include <kritva/hardware/typed_endpoints.hpp>

using namespace kritva::hardware;
using namespace kritva::hardware::test;
using kritva::core::ErrorCode;
using kritva::core::LifecycleState;
namespace conf = kritva::hardware::conformance;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

template <class T>
constexpr bool plain = std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T>;
static_assert(plain<Vec3> && plain<AccelerationSample> && plain<AngularVelocitySample> && plain<PositionSample> && plain<MotorCommand> && plain<MotorLimits>,
              "samples and commands are plain value types");

static void test_default_state() {                                   // DER-505
    const AccelerationSample a;
    KRITVA_CHECK(a.value.x == 0 && a.value.y == 0 && a.value.z == 0 && a.sequence == 0 && a.timestamp.nanoseconds() == 0);
    const AngularVelocitySample w;
    KRITVA_CHECK(w.value.x == 0 && w.sequence == 0);
    const PositionSample p;
    KRITVA_CHECK(p.value == 0 && p.sequence == 0 && p.timestamp.nanoseconds() == 0);
    KRITVA_CHECK(MotorCommand{}.velocity_rad_s == 0);
    KRITVA_CHECK(MotorLimits{}.min_rad_s == -1.0 && MotorLimits{}.max_rad_s == 1.0);
    KRITVA_CHECK(is_valid(a) && is_valid(w) && is_valid(p) && is_valid(MotorCommand{}));   // the default is valid
}

static void test_sample_validity() {
    AccelerationSample a;
    KRITVA_CHECK(is_valid(a));
    for (double bad : {kNaN, kInf, -kInf}) {
        for (int axis = 0; axis < 3; ++axis) {
            a = AccelerationSample{};
            (axis == 0 ? a.value.x : axis == 1 ? a.value.y : a.value.z) = bad;
            KRITVA_CHECK(!is_valid(a));
        }
        AngularVelocitySample w; w.value.z = bad; KRITVA_CHECK(!is_valid(w));
        PositionSample p; p.value = bad; KRITVA_CHECK(!is_valid(p));
        MotorCommand c; c.velocity_rad_s = bad; KRITVA_CHECK(!is_valid(c));
    }
    PositionSample big; big.value = 1e300;
    KRITVA_CHECK(is_valid(big));                                                  // large but finite is valid
}

static void test_motor_command_validation() {                        // DER-502, safety
    const MotorLimits limits{-2.0, 3.0};
    for (double ok : {0.0, -0.0, 1.5, -2.0, 3.0, 2.999999}) KRITVA_CHECK(validate(MotorCommand{ok}, limits).has_value());   // bounds inclusive
    for (double bad : {3.0000001, -2.0000001, 1e300, -1e300, kInf, -kInf, kNaN}) {
        const auto r = validate(MotorCommand{bad}, limits);
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
    }
    KRITVA_CHECK(validate(MotorCommand{kNaN}, limits).error().message == "motor command must be a finite number");
    KRITVA_CHECK(validate(MotorCommand{9.0}, limits).error().message == "motor command is outside the allowed range");
    KRITVA_CHECK(validate(MotorCommand{0.5}, MotorLimits{}).has_value());          // default [-1, 1]
    KRITVA_CHECK(!validate(MotorCommand{1.5}, MotorLimits{}).has_value());
}

static void test_limit_validation() {
    KRITVA_CHECK(validate_limits(MotorLimits{-1, 1}).has_value() && validate_limits(MotorLimits{0, 0}).has_value());
    for (const MotorLimits& bad : {MotorLimits{2, 1}, MotorLimits{kNaN, 1}, MotorLimits{-1, kNaN}, MotorLimits{-kInf, 1}, MotorLimits{-1, kInf}}) {
        const auto r = validate_limits(bad);
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
    }
    // Limits that are unusable reject every command (including zero is not special-cased).
    KRITVA_CHECK(!validate(MotorCommand{0.0}, MotorLimits{2, 1}).has_value());
}

static void test_virtual_timestamp() {                               // DER-702
    KRITVA_CHECK(virtual_timestamp(0).nanoseconds() == 0 && virtual_timestamp(1).nanoseconds() == 1'000'000);
    KRITVA_CHECK(virtual_timestamp(5).nanoseconds() == 5'000'000 && virtual_timestamp(3, 250).nanoseconds() == 750);
    KRITVA_CHECK(virtual_timestamp(7).domain() == kritva::core::ClockDomain::MONOTONIC);
    KRITVA_CHECK(virtual_timestamp(9) == virtual_timestamp(9));                  // same input, same stamp
    KRITVA_CHECK(virtual_timestamp(2).nanoseconds() < virtual_timestamp(3).nanoseconds());
}

static void test_capability_descriptions() {
    for (const auto& caps : {acceleration_capability(), angular_velocity_capability(), position_capability(), motor_command_capability()}) {
        KRITVA_CHECK(caps.size() == 1 && !caps.all()[0].name.empty());
    }
    KRITVA_CHECK(acceleration_capability().contains(kCapabilityAcceleration) && motor_command_capability().contains(kCapabilityMotorCommand));
    KRITVA_CHECK(kCapabilityAcceleration != kCapabilityAngularVelocity && kCapabilityPosition != kCapabilityMotorCommand);
}

// Generic deterministic doubles of the typed endpoints (the I3-005 mocks come later).
template <class Sample>
class TypedSensor : public Instrumented<SensorEndpoint<Sample>> {
public:
    TypedSensor(std::uint64_t id, const char* name, kritva::core::CapabilitySet caps)
        : Instrumented<SensorEndpoint<Sample>>(make_info(id, name, EndpointDirection::SENSOR), std::move(caps)) {}
protected:
    kritva::core::Result<void> do_read(Sample& out) override {
        out = Sample{};
        out.sequence = ++produced_;
        out.timestamp = virtual_timestamp(out.sequence);
        return kritva::core::Result<void>::success();
    }
private:
    std::uint64_t produced_{0};
};

class MotorDouble : public Instrumented<MotorCommandEndpoint> {
public:
    MotorDouble() : Instrumented<MotorCommandEndpoint>(make_info(1, "command", EndpointDirection::ACTUATOR), motor_command_capability()) {}
    MotorLimits limits{-2.0, 2.0};
    int applied_count{0};
    MotorCommand last{};
protected:
    kritva::core::Result<void> do_write(const MotorCommand& c) override {
        if (auto r = validate(c, limits); !r) return r;                          // validate before acting
        last = c;
        ++applied_count;
        return kritva::core::Result<void>::success();
    }
};

template <class Sample>
static void conforms_as_sensor(kritva::core::CapabilitySet caps) {
    using E = TypedSensor<Sample>;
    conf::Fixture<SensorEndpoint<Sample>> f{[caps] { return std::make_unique<E>(1, "sensor", caps); },
                                            [](SensorEndpoint<Sample>& e) { return static_cast<E&>(e).inject_fault(); }};
    const auto v = conf::check_sensor_contract<Sample>(f);
    if (!v.empty()) { std::fprintf(stderr, "conformance violation: %s\n", v.c_str()); std::exit(1); }
}

static void test_typed_endpoints_conform() {                         // DER-501..504
    conforms_as_sensor<AccelerationSample>(acceleration_capability());
    conforms_as_sensor<AngularVelocitySample>(angular_velocity_capability());
    conforms_as_sensor<PositionSample>(position_capability());
    conf::Fixture<MotorCommandEndpoint> f{[] { return std::make_unique<MotorDouble>(); },
                                          [](MotorCommandEndpoint& e) { return static_cast<MotorDouble&>(e).inject_fault(); }};
    const auto v = conf::check_actuator_contract<MotorCommand>(f);
    if (!v.empty()) { std::fprintf(stderr, "conformance violation: %s\n", v.c_str()); std::exit(1); }
}

static void test_operation_before_ready_and_after_stop() {           // DER-503, DER-504
    TypedSensor<AccelerationSample> s(1, "accel", acceleration_capability());
    AccelerationSample out;
    out.sequence = 77;
    auto r = s.read(out);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::NOT_READY && out.sequence == 77);   // before ready: untouched
    KRITVA_CHECK(s.initialize().has_value());
    KRITVA_CHECK(s.read(out).error().code == ErrorCode::NOT_READY);                                 // READY is not RUNNING
    KRITVA_CHECK(s.start().has_value() && s.read(out).has_value() && out.sequence == 1);
    KRITVA_CHECK(s.stop().has_value());
    r = s.read(out);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::NOT_READY && out.sequence == 1);    // after stop
}

static void test_sequence_and_timestamps_are_deterministic() {
    auto run = [] {
        TypedSensor<PositionSample> s(1, "p", position_capability());
        KRITVA_CHECK(s.initialize().has_value() && s.start().has_value());
        std::vector<std::pair<std::uint64_t, std::int64_t>> out;
        for (int i = 0; i < 5; ++i) { PositionSample p; KRITVA_CHECK(s.read(p).has_value()); out.emplace_back(p.sequence, p.timestamp.nanoseconds()); }
        return out;
    };
    const auto a = run();
    KRITVA_CHECK(a == run());                                                      // identical runs
    for (std::size_t i = 0; i < a.size(); ++i) KRITVA_CHECK(a[i].first == i + 1 && a[i].second == static_cast<std::int64_t>(i + 1) * 1'000'000);
}

static void test_rejected_commands_have_no_effect() {                // safety
    MotorDouble m;
    KRITVA_CHECK(m.initialize().has_value() && m.start().has_value());
    KRITVA_CHECK(m.write(MotorCommand{1.5}).has_value() && m.applied_count == 1 && m.last.velocity_rad_s == 1.5);
    for (double bad : {kNaN, kInf, -kInf, 2.5, -2.5}) {
        const auto r = m.write(MotorCommand{bad});
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
        KRITVA_CHECK(m.applied_count == 1 && m.last.velocity_rad_s == 1.5);        // the last good command stays
    }
    KRITVA_CHECK(m.statistics().sample_count.value() == 1 && m.statistics().error_count.value() == 5);
    KRITVA_CHECK(m.lifecycle_state() == LifecycleState::RUNNING);                  // rejection does not fault the endpoint
    KRITVA_CHECK(m.inject_fault().has_value());
    KRITVA_CHECK(m.write(MotorCommand{0.5}).error().code == ErrorCode::RESOURCE_UNAVAILABLE && m.applied_count == 1);
}

int main() {
    test_default_state();
    test_sample_validity();
    test_motor_command_validation();
    test_limit_validation();
    test_virtual_timestamp();
    test_capability_descriptions();
    test_typed_endpoints_conform();
    test_operation_before_ready_and_after_stop();
    test_sequence_and_timestamps_are_deterministic();
    test_rejected_commands_have_no_effect();
    std::printf("typed_data_test: PASS\n");
    return 0;
}
