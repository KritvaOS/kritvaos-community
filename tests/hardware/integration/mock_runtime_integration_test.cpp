//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : mock_runtime_integration_test.cpp
// Description : Integration: mock devices configured from text, run by the DeviceManager inside the RuntimeHost.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-205; DER-702; DER-803; DER-805
// API         : HARDWARE-MOCK-IT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <string>
#include <vector>

#include "../../runtime/check.hpp"

#include <kritva/hardware/device_manager.hpp>
#include <kritva/hardware/mock/mock_imu.hpp>
#include <kritva/hardware/mock/mock_motor.hpp>
#include <kritva/runtime/runtime_host.hpp>

using namespace kritva;
using namespace kritva::hardware;
using namespace kritva::hardware::mock;
using kritva::core::EventType;
using kritva::core::HealthState;
using kritva::core::LifecycleState;

struct Rig {
    MockImuDevice imu{DeviceInfo::create(DeviceId{1}, "left_arm_imu").value()};
    MockMotorDevice motor{DeviceInfo::create(DeviceId{2}, "shoulder_motor").value()};
    DeviceManager manager{core::runtime::ComponentId{100}};
    runtime::RuntimeHost host;
    runtime::EventLog events;

    Rig() {
        KRITVA_CHECK(manager.register_device(imu).has_value() && manager.register_device(motor).has_value());
        manager.set_event_sink(&events);
        host.set_event_sink(&events);
        KRITVA_CHECK(host.add_component(manager).has_value());
    }
    void configure(const std::string& text) {
        auto keys = manager.config_keys();
        keys.push_back("runtime.name");
        const auto cfg = runtime::parse_configuration(text, keys);
        if (!cfg) { std::fprintf(stderr, "config: %s\n", cfg.error().message.c_str()); std::exit(1); }
        KRITVA_CHECK(host.configure(cfg.value()).has_value());
    }
};

static void test_config_keys_cover_the_mocks() {
    Rig r;
    const auto keys = r.manager.config_keys();
    auto has = [&](const char* k) { for (const auto& x : keys) if (x == k) return true; return false; };
    for (const char* k : {"left_arm_imu.enabled", "left_arm_imu.acceleration.start", "left_arm_imu.acceleration.step",
                          "left_arm_imu.angular_velocity.fault_after_ops", "shoulder_motor.command.min_rad_s",
                          "shoulder_motor.command.max_rad_s", "shoulder_motor.command.fail_after_writes",
                          "shoulder_motor.command.fault_after_writes", "shoulder_motor.position.initial"}) {
        KRITVA_CHECK(has(k));
    }
    const auto typo = runtime::parse_configuration("runtime.name=x\nshoulder_motor.command.max_rad=2\n", keys);
    KRITVA_CHECK(!typo.has_value());
}

static void test_configured_run() {
    Rig r;
    r.configure("runtime.name=mock\nleft_arm_imu.acceleration.start=100\nleft_arm_imu.acceleration.step=5\n"
                "shoulder_motor.command.max_rad_s=4\nshoulder_motor.position.initial=1\n");
    KRITVA_CHECK(r.host.initialize().has_value() && r.host.start().has_value());
    AccelerationSample a;
    KRITVA_CHECK(r.imu.acceleration().read(a).has_value() && a.value.x == 100.0);
    KRITVA_CHECK(r.imu.acceleration().read(a).has_value() && a.value.x == 105.0);
    KRITVA_CHECK(r.motor.command().write(MotorCommand{3.5}).has_value());
    KRITVA_CHECK(!r.motor.command().write(MotorCommand{4.5}).has_value());                       // the configured limit is in force
    PositionSample p;
    KRITVA_CHECK(r.motor.position().read(p).has_value() && p.value > 1.0);
    KRITVA_CHECK(r.host.controlled_shutdown().has_value() && r.host.state() == LifecycleState::STOPPED);
    KRITVA_CHECK(r.motor.model().velocity_rad_s == 0.0);                                         // the joint is stopped at shutdown
}

static void test_scheduled_fault_is_observed_through_the_runtime() {
    Rig r;
    r.configure("runtime.name=mock\nleft_arm_imu.angular_velocity.fault_after_ops=2\n");
    KRITVA_CHECK(r.host.initialize().has_value() && r.host.start().has_value());
    r.events.clear();
    AngularVelocitySample w;
    KRITVA_CHECK(r.imu.angular_velocity().read(w).has_value() && !r.host.failure_report().value().any());
    KRITVA_CHECK(r.imu.angular_velocity().read(w).has_value());                                  // the second read triggers the fault
    const auto report = r.host.failure_report().value();
    KRITVA_CHECK(report.any() && report.failed[0] == r.manager.info().id());
    KRITVA_CHECK(r.host.state() == LifecycleState::RUNNING);
    const auto ev = r.events.snapshot();
    KRITVA_CHECK(ev.size() == 1 && ev[0].type == EventType::ERROR && ev[0].source_id == r.manager.info().id());
    KRITVA_CHECK(!r.imu.angular_velocity().read(w).has_value());                                 // no silent recovery
    AccelerationSample a;
    KRITVA_CHECK(r.imu.acceleration().read(a).has_value());                                      // the rest of the device still works
    KRITVA_CHECK(r.host.controlled_shutdown().has_value() && r.host.state() == LifecycleState::STOPPED);
}

static void test_disabled_device_and_determinism() {
    auto run = [] {
        Rig r;
        r.configure("runtime.name=mock\nshoulder_motor.enabled=false\n");
        KRITVA_CHECK(r.host.initialize().has_value() && r.host.start().has_value());
        KRITVA_CHECK(r.motor.command().lifecycle_state() == LifecycleState::UNKNOWN);
        std::vector<double> out;
        for (int i = 0; i < 4; ++i) { AccelerationSample a; KRITVA_CHECK(r.imu.acceleration().read(a).has_value()); out.push_back(a.value.x); }
        KRITVA_CHECK(r.host.controlled_shutdown().has_value());
        return out;
    };
    KRITVA_CHECK(run() == run());
}

int main() {
    test_config_keys_cover_the_mocks();
    test_configured_run();
    test_scheduled_fault_is_observed_through_the_runtime();
    test_disabled_device_and_determinism();
    std::printf("mock_runtime_integration_test: PASS\n");
    return 0;
}
