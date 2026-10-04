//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : typed_discovery_integration_test.cpp
// Description : Integration: typed endpoints discovered through the registry and operated under the DeviceManager.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-105; DER-106; DER-501; DER-502; DER-503; DER-504
// API         : HARDWARE-TYPED-IT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <memory>

#include "../../runtime/check.hpp"
#include "../support/test_endpoints.hpp"

#include <kritva/hardware/device_manager.hpp>
#include <kritva/hardware/typed_endpoints.hpp>

using namespace kritva::hardware;
using namespace kritva::hardware::test;
using kritva::core::ErrorCode;

class Accel final : public AccelerationEndpoint {
public:
    Accel() : AccelerationEndpoint(make_info(1, "acceleration", EndpointDirection::SENSOR), acceleration_capability()) {}
protected:
    kritva::core::Result<void> do_read(AccelerationSample& out) override {
        out = AccelerationSample{};
        out.sequence = ++n_;
        out.value = Vec3{0.0, 0.0, 9.81};
        out.timestamp = virtual_timestamp(n_);
        return kritva::core::Result<void>::success();
    }
private:
    std::uint64_t n_{0};
};

class Motor final : public MotorCommandEndpoint {
public:
    Motor() : MotorCommandEndpoint(make_info(1, "command", EndpointDirection::ACTUATOR), motor_command_capability()) {}
    MotorCommand last{};
protected:
    kritva::core::Result<void> do_write(const MotorCommand& c) override {
        if (auto r = validate(c, MotorLimits{-2.0, 2.0}); !r) return r;
        last = c;
        return kritva::core::Result<void>::success();
    }
};

int main() {
    Device imu(DeviceInfo::create(DeviceId{1}, "left_arm_imu").value());
    Device motor(DeviceInfo::create(DeviceId{2}, "shoulder_motor").value());
    KRITVA_CHECK(imu.add_endpoint(std::make_unique<Accel>()).has_value());
    KRITVA_CHECK(motor.add_endpoint(std::make_unique<Motor>()).has_value());

    DeviceManager manager{kritva::core::runtime::ComponentId{100}};
    KRITVA_CHECK(manager.register_device(imu).has_value() && manager.register_device(motor).has_value());

    // Discovery by name yields the typed interface; the wrong type is detectable and not mistaken.
    auto* accel = dynamic_cast<AccelerationEndpoint*>(manager.registry().find_endpoint("left_arm_imu", "acceleration").value());
    auto* command = dynamic_cast<MotorCommandEndpoint*>(manager.registry().find_endpoint("shoulder_motor", "command").value());
    KRITVA_CHECK(accel != nullptr && command != nullptr);
    KRITVA_CHECK(dynamic_cast<PositionEndpoint*>(manager.registry().find_endpoint("left_arm_imu", "acceleration").value()) == nullptr);
    KRITVA_CHECK(manager.capabilities().contains(kCapabilityAcceleration) && manager.capabilities().contains(kCapabilityMotorCommand));

    AccelerationSample s;
    KRITVA_CHECK(accel->read(s).error().code == ErrorCode::NOT_READY && s.sequence == 0);          // before the runtime started it
    KRITVA_CHECK(command->write(MotorCommand{1.0}).error().code == ErrorCode::NOT_READY);

    KRITVA_CHECK(manager.initialize().has_value() && manager.start().has_value());
    KRITVA_CHECK(accel->read(s).has_value() && s.sequence == 1 && s.value.z == 9.81 && s.timestamp.nanoseconds() == 1'000'000 && is_valid(s));
    KRITVA_CHECK(accel->read(s).has_value() && s.sequence == 2);
    KRITVA_CHECK(command->write(MotorCommand{1.5}).has_value());
    KRITVA_CHECK(!command->write(MotorCommand{5.0}).has_value());
    KRITVA_CHECK(static_cast<Motor*>(command)->last.velocity_rad_s == 1.5);                       // the rejected command was not applied
    KRITVA_CHECK(manager.statistics().sample_count.value() == 3 && manager.statistics().error_count.value() == 3);

    KRITVA_CHECK(manager.stop().has_value());
    KRITVA_CHECK(accel->read(s).error().code == ErrorCode::NOT_READY && command->write(MotorCommand{0.5}).error().code == ErrorCode::NOT_READY);   // after stop
    KRITVA_CHECK(manager.shutdown().has_value());
    std::printf("typed_discovery_integration_test: PASS\n");
    return 0;
}
