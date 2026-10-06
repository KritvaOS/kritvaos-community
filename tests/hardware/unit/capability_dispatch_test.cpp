//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : capability_dispatch_test.cpp
// Description : Unit tests of the capability-to-typed-endpoint dispatch.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: ER-002; ER-003
// API         : CAPABILITY-DISPATCH-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include "../../runtime/check.hpp"
#include "../support/test_endpoints.hpp"

#include <kritva/hardware/mock/mock_imu.hpp>
#include <kritva/hardware/mock/mock_motor.hpp>
#include <kritva/hardware/remote/capability_dispatch.hpp>

using namespace kritva::hardware;
using namespace kritva::hardware::remote;
using kritva::core::ErrorCode;

static void test_classification_of_the_mock_endpoints() {
    mock::MockImuDevice imu(DeviceInfo::create(DeviceId{1}, "imu").value());
    mock::MockMotorDevice motor(DeviceInfo::create(DeviceId{2}, "motor").value());
    KRITVA_CHECK(classify(imu.acceleration()) == EndpointKind::ACCELERATION);
    KRITVA_CHECK(classify(imu.angular_velocity()) == EndpointKind::ANGULAR_VELOCITY);
    KRITVA_CHECK(classify(motor.position()) == EndpointKind::POSITION);
    KRITVA_CHECK(classify(motor.command()) == EndpointKind::MOTOR_COMMAND);
    KRITVA_CHECK(capability_id_of(EndpointKind::ACCELERATION) == 0x1001 && capability_id_of(EndpointKind::ANGULAR_VELOCITY) == 0x1002 &&
                 capability_id_of(EndpointKind::POSITION) == 0x1003 && capability_id_of(EndpointKind::MOTOR_COMMAND) == 0x2001);
    // The wire capability ids are the I3 ones.
    KRITVA_CHECK(capability_id_of(EndpointKind::ACCELERATION) == kCapabilityAcceleration.value() && capability_id_of(EndpointKind::MOTOR_COMMAND) == kCapabilityMotorCommand.value());
}

static void test_unsupported_endpoints_are_not_classified() {            // no silent generic endpoint
    // An id from the table on an object that is not the typed endpoint.
    test::TestSensor lookalike(0x1001, "lookalike");
    KRITVA_CHECK(!classify(lookalike));
    test::TestActuator motor_lookalike(0x2001, "lookalike");              // 0x2001, but ActuatorEndpoint<TestCommand>
    KRITVA_CHECK(!classify(motor_lookalike));
    // An unknown capability id.
    test::TestSensor unknown(0x1004, "unknown");
    KRITVA_CHECK(!classify(unknown));
    test::TestEndpoint plain(7, "plain", EndpointDirection::SENSOR);
    KRITVA_CHECK(!classify(plain));
    // No capability and two capabilities.
    class NoCapability final : public Endpoint {
    public:
        NoCapability() : Endpoint(EndpointInfo::create(EndpointId{1}, "none", EndpointDirection::SENSOR).value(), {}) {}
    } none;
    KRITVA_CHECK(!classify(none));
    class TwoCapabilities final : public SensorEndpoint<PositionSample> {
    public:
        TwoCapabilities() : SensorEndpoint<PositionSample>(EndpointInfo::create(EndpointId{1}, "two", EndpointDirection::SENSOR).value(), caps()) {}
        static kritva::core::CapabilitySet caps() {
            kritva::core::CapabilitySet set;
            set.add(kritva::core::Capability{kCapabilityPosition, "position", {}});
            set.add(kritva::core::Capability{kCapabilityAcceleration, "acceleration", {}});
            return set;
        }
    protected:
        kritva::core::Result<void> do_read(PositionSample&) override { return kritva::core::Result<void>::success(); }
    } two;
    KRITVA_CHECK(!classify(two));                                          // exactly one capability is required
}

static void test_read_and_write_are_refused_on_the_wrong_kind() {
    mock::MockImuDevice imu(DeviceInfo::create(DeviceId{1}, "imu").value());
    mock::MockMotorDevice motor(DeviceInfo::create(DeviceId{2}, "motor").value());
    test::TestSensor lookalike(0x1001, "lookalike");
    KRITVA_CHECK(read_sensor(motor.command()).error().code == ErrorCode::INVALID_ARGUMENT);   // an actuator is not a sensor
    KRITVA_CHECK(read_sensor(lookalike).error().code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(write_motor_command(imu.acceleration(), 0.1).error().code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(write_motor_command(motor.position(), 0.1).error().code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(motor.command().statistics().sample_count.value() == 0 && motor.model().velocity_rad_s == 0.0);
}

static void run_to_running(Endpoint& e) {
    KRITVA_CHECK(e.configure(kritva::core::Configuration{}).has_value() && e.initialize().has_value() && e.start().has_value());
}

static void test_reads_and_writes_use_the_endpoints_own_operations() {
    mock::MockImuDevice imu(DeviceInfo::create(DeviceId{1}, "imu").value());
    mock::MockMotorDevice motor(DeviceInfo::create(DeviceId{2}, "motor").value());
    // Not running: the I3 state check answers, unchanged.
    KRITVA_CHECK(read_sensor(imu.acceleration()).error().code == ErrorCode::NOT_READY);
    KRITVA_CHECK(write_motor_command(motor.command(), 0.1).error().code == ErrorCode::NOT_READY);
    run_to_running(imu.acceleration());
    run_to_running(imu.angular_velocity());
    run_to_running(motor.command());
    run_to_running(motor.position());

    const auto a = read_sensor(imu.acceleration());
    KRITVA_CHECK(a.has_value() && a.value().kind == transport::ReadKind::VEC3 && a.value().sample_sequence == 1);
    AccelerationSample direct;
    KRITVA_CHECK(imu.acceleration().read(direct).has_value() && direct.sequence == 2);
    const auto a3 = read_sensor(imu.acceleration());
    KRITVA_CHECK(a3.value().sample_sequence == 3 && a3.value().timestamp_ns > a.value().timestamp_ns);
    const auto g = read_sensor(imu.angular_velocity());
    KRITVA_CHECK(g.has_value() && g.value().kind == transport::ReadKind::VEC3 && g.value().sample_sequence == 1);
    const auto p = read_sensor(motor.position());
    KRITVA_CHECK(p.has_value() && p.value().kind == transport::ReadKind::SCALAR && p.value().sample_sequence == 1);

    KRITVA_CHECK(write_motor_command(motor.command(), 0.5).has_value() && motor.model().velocity_rad_s == 0.5);
    KRITVA_CHECK(write_motor_command(motor.command(), 3.0).error().code == ErrorCode::INVALID_ARGUMENT && motor.model().velocity_rad_s == 0.5);   // I3 limits
    imu.acceleration().fail_next_operation();
    const auto failed = read_sensor(imu.acceleration());
    KRITVA_CHECK(!failed.has_value() && failed.error().code == ErrorCode::INTERNAL_ERROR && failed.error().message == "injected read failure");
}

int main() {
    test_classification_of_the_mock_endpoints();
    test_unsupported_endpoints_are_not_classified();
    test_read_and_write_are_refused_on_the_wrong_kind();
    test_reads_and_writes_use_the_endpoints_own_operations();
    std::printf("capability_dispatch_test: PASS\n");
    return 0;
}
