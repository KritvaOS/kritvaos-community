//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device_manager_sanity.cpp
// Description : Sanity executable: DeviceManager lifecycle, endpoint fault and controlled shutdown through the runtime.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-401..407; DER-607; DER-704
// API         : HARDWARE-MANAGER-SANITY
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <cstdio>
#include <memory>
#include <string>

#include "../../runtime/check.hpp"
#include "../support/test_endpoints.hpp"

#include <kritva/hardware/device_manager.hpp>
#include <kritva/runtime/runtime_host.hpp>

using namespace kritva;
using namespace kritva::hardware;
using namespace kritva::hardware::test;

int main() {
    Device imu(DeviceInfo::create(DeviceId{1}, "imu").value());
    auto* gyro = static_cast<TestSensor*>(imu.add_endpoint(std::make_unique<TestSensor>(1, "gyro")).value());
    DeviceManager manager{core::runtime::ComponentId{100}};
    KRITVA_CHECK(manager.register_device(imu).has_value());

    runtime::RuntimeHost host;
    runtime::EventLog events;
    host.set_event_sink(&events);
    manager.set_event_sink(&events);
    KRITVA_CHECK(host.add_component(manager).has_value());
    KRITVA_CHECK(host.configure(runtime::parse_configuration("runtime.name=sanity\n").value()).has_value());

    const auto show = [&](const char* step) {
        std::printf("[manager] %s: runtime=%s endpoint=%s\n", step, std::string(runtime::to_string(host.state())).c_str(),
                    std::string(runtime::to_string(gyro->lifecycle_state())).c_str());
    };
    KRITVA_CHECK(host.initialize().has_value());
    show("initialized");
    KRITVA_CHECK(host.start().has_value());
    show("started");
    gyro->inject_fault("gyro lost");
    show("faulted");
    std::printf("[manager] failure_report failed=%zu\n", host.failure_report().value().failed.size());
    KRITVA_CHECK(host.controlled_shutdown().has_value());
    show("shutdown");
    return 0;
}
