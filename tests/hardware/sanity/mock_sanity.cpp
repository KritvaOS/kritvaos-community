//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : mock_sanity.cpp
// Description : Sanity executable: mock devices run under the runtime and print deterministic data.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-702; DER-802; DER-803
// API         : HARDWARE-MOCK-SANITY
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <cstdio>

#include "../../runtime/check.hpp"

#include <kritva/hardware/device_manager.hpp>
#include <kritva/hardware/mock/mock_imu.hpp>
#include <kritva/hardware/mock/mock_motor.hpp>
#include <kritva/runtime/runtime_host.hpp>

using namespace kritva;
using namespace kritva::hardware;
using namespace kritva::hardware::mock;

int main() {
    MockImuDevice imu(DeviceInfo::create(DeviceId{1}, "left_arm_imu").value());
    MockMotorDevice motor(DeviceInfo::create(DeviceId{2}, "shoulder_motor").value());
    DeviceManager manager{core::runtime::ComponentId{100}};
    KRITVA_CHECK(manager.register_device(imu).has_value() && manager.register_device(motor).has_value());
    runtime::RuntimeHost host;
    KRITVA_CHECK(host.add_component(manager).has_value());
    KRITVA_CHECK(host.configure(runtime::parse_configuration("runtime.name=mock_sanity\n").value()).has_value());
    KRITVA_CHECK(host.initialize().has_value() && host.start().has_value());

    for (int i = 0; i < 3; ++i) {
        AccelerationSample a;
        AngularVelocitySample w;
        KRITVA_CHECK(imu.acceleration().read(a).has_value() && imu.angular_velocity().read(w).has_value());
        std::printf("[mock] imu n=%llu accel=(%.1f,%.1f,%.2f) gyro_x=%.1f t=%lldns\n", static_cast<unsigned long long>(a.sequence),
                    a.value.x, a.value.y, a.value.z, w.value.x, static_cast<long long>(a.timestamp.nanoseconds()));
    }
    KRITVA_CHECK(motor.command().write(MotorCommand{0.5}).has_value());
    const auto bad = motor.command().write(MotorCommand{5.0});
    std::printf("[mock] motor write 0.5 ok, write 5.0 rejected=%d\n", bad.has_value() ? 0 : 1);
    PositionSample p;
    KRITVA_CHECK(motor.position().read(p).has_value());
    std::printf("[mock] position=%.3f\n", p.value);
    KRITVA_CHECK(motor.command().inject_fault().has_value());
    std::printf("[mock] failure_report failed=%zu\n", host.failure_report().value().failed.size());
    KRITVA_CHECK(host.controlled_shutdown().has_value());
    std::printf("[mock] shutdown state=%s\n", std::string(runtime::to_string(host.state())).c_str());
    return 0;
}
