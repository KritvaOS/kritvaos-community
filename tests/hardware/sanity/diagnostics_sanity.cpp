//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : diagnostics_sanity.cpp
// Description : Sanity executable: prints device diagnostics before, during and after an endpoint fault.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-601..607; DER-701
// API         : HARDWARE-DIAGNOSTICS-SANITY
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <cstdio>
#include <string>

#include "../../runtime/check.hpp"

#include <kritva/hardware/device_manager.hpp>
#include <kritva/hardware/mock/mock_imu.hpp>
#include <kritva/runtime/runtime_host.hpp>

using namespace kritva;
using namespace kritva::hardware;
using namespace kritva::hardware::mock;

int main() {
    MockImuDevice imu(DeviceInfo::create(DeviceId{1}, "left_arm_imu").value());
    DeviceManager manager{core::runtime::ComponentId{100}};
    KRITVA_CHECK(manager.register_device(imu).has_value());
    runtime::RuntimeHost host;
    KRITVA_CHECK(host.add_component(manager).has_value());
    KRITVA_CHECK(host.configure(runtime::parse_configuration("runtime.name=diag\n").value()).has_value());
    KRITVA_CHECK(host.initialize().has_value() && host.start().has_value());

    AccelerationSample a;
    KRITVA_CHECK(imu.acceleration().read(a).has_value());
    std::printf("[diag] running\n%s", describe(manager.diagnostics()).c_str());
    KRITVA_CHECK(imu.angular_velocity().inject_fault("gyro lost").has_value());
    std::printf("[diag] faulted\n%s", describe(manager.diagnostics()).c_str());
    KRITVA_CHECK(host.controlled_shutdown().has_value());
    std::printf("[diag] shutdown\n%s", describe(manager.diagnostics()).c_str());
    return 0;
}
