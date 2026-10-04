//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : registry_discovery_integration_test.cpp
// Description : Integration: discover endpoints through the registry and operate them through the Device contracts.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-105; DER-106; DER-205; DER-401..403
// API         : HARDWARE-REGISTRY-IT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <memory>

#include "../../runtime/check.hpp"
#include "../support/test_endpoints.hpp"

using namespace kritva::hardware;
using namespace kritva::hardware::test;
using kritva::core::LifecycleState;

int main() {
    Device imu(DeviceInfo::create(DeviceId{1}, "left_arm_imu").value());
    Device motor(DeviceInfo::create(DeviceId{2}, "shoulder_motor").value());
    KRITVA_CHECK(imu.add_endpoint(std::make_unique<TestSensor>(1, "acceleration")).has_value());
    KRITVA_CHECK(imu.add_endpoint(std::make_unique<TestSensor>(2, "angular_velocity")).has_value());
    KRITVA_CHECK(motor.add_endpoint(std::make_unique<TestActuator>(1, "command")).has_value());
    KRITVA_CHECK(motor.add_endpoint(std::make_unique<TestSensor>(2, "position")).has_value());

    DeviceRegistry registry;
    KRITVA_CHECK(registry.register_device(imu).has_value() && registry.register_device(motor).has_value());

    // Discovery by name returns typed endpoints that can be operated through the common contract.
    auto* accel = dynamic_cast<SensorEndpoint<TestSample>*>(registry.find_endpoint("left_arm_imu", "acceleration").value());
    auto* command = dynamic_cast<ActuatorEndpoint<TestCommand>*>(registry.find_endpoint("shoulder_motor", "command").value());
    KRITVA_CHECK(accel != nullptr && command != nullptr);
    KRITVA_CHECK(dynamic_cast<ActuatorEndpoint<TestCommand>*>(registry.find_endpoint("left_arm_imu", "acceleration").value()) == nullptr);   // wrong type is detectable

    for (Device* d : registry.devices()) {                                         // deterministic order
        for (Endpoint* e : d->endpoints()) KRITVA_CHECK(e->initialize().has_value() && e->start().has_value());
    }
    TestSample s;
    KRITVA_CHECK(accel->read(s).has_value() && s.value == 1);
    KRITVA_CHECK(command->write(TestCommand{7}).has_value());
    KRITVA_CHECK(static_cast<TestActuator*>(command)->applied->value == 7);

    // Discovery by id reaches the very same objects.
    KRITVA_CHECK(registry.find_endpoint(DeviceId{1}, EndpointId{1}).value() == accel);
    KRITVA_CHECK(registry.find_endpoint(DeviceId{2}, EndpointId{1}).value() == command);
    KRITVA_CHECK(imu.health().state() == kritva::core::HealthState::HEALTHY);
    std::printf("registry_discovery_integration_test: PASS\n");
    return 0;
}
