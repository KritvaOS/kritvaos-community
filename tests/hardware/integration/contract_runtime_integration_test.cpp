//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : contract_runtime_integration_test.cpp
// Description : Integration: the contracts fit the KOS-I2 runtime through a Core Component (preview of the DeviceManager).
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-004; DER-403; DER-405; DER-406; DER-407; DER-704
// API         : HARDWARE-RUNTIME-IT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <memory>
#include <string>
#include <vector>

#include "../../runtime/check.hpp"
#include "../support/test_endpoints.hpp"

#include <kritva/runtime/runtime_host.hpp>

using namespace kritva;
using namespace kritva::hardware;
using namespace kritva::hardware::test;
using kritva::core::HealthState;
using kritva::core::LifecycleState;

// A test-only Core Component that drives one Device's endpoints in order. It only
// shows that the I3-001 contracts can be coordinated by the existing runtime
// without changing it; the real DeviceManager is I3-003.
class DeviceHost final : public core::runtime::Component {
public:
    explicit DeviceHost(Device& device)
        : Component(core::runtime::ComponentInfo::create(core::runtime::ComponentId{50}, "device_host").value()), device_(device) {}

    core::Result<void> configure(const core::Configuration& c) override { return each(false, [&](Endpoint& e) { return e.configure(c); }); }
    core::Result<void> initialize() override { return each(false, [](Endpoint& e) { return e.initialize(); }); }
    core::Result<void> start() override { return each(false, [](Endpoint& e) { return e.start(); }); }
    core::Result<void> stop() override {
        return each(true, [](Endpoint& e) {
            return e.lifecycle_state() == LifecycleState::FAULT ? core::Result<void>::success() : e.stop();   // faulted: released by shutdown
        });
    }
    core::Result<void> shutdown() override { return each(true, [](Endpoint& e) { return e.shutdown(); }); }
    core::LifecycleState lifecycle_state() const noexcept override { return device_.endpoints().empty() ? LifecycleState::UNKNOWN : device_.endpoints().front()->lifecycle_state(); }
    core::Status status() const override { return device_.status(); }
    core::Health health() const override { return device_.health(); }
    core::CapabilitySet capabilities() const override { return device_.capabilities(); }

private:
    template <class F>
    core::Result<void> each(bool reverse, F op) {
        const auto& eps = device_.endpoints();
        if (reverse) { for (auto it = eps.rbegin(); it != eps.rend(); ++it) if (auto r = op(**it); !r) return r; }
        else         { for (auto* e : eps) if (auto r = op(*e); !r) return r; }
        return core::Result<void>::success();
    }
    Device& device_;
};

int main() {
    Device device(DeviceInfo::create(DeviceId{1}, "left_arm_imu").value());
    auto* accel = static_cast<TestSensor*>(device.add_endpoint(std::make_unique<TestSensor>(1, "acceleration")).value());
    auto* gyro = static_cast<TestSensor*>(device.add_endpoint(std::make_unique<TestSensor>(2, "angular_velocity")).value());

    runtime::RuntimeHost host;
    DeviceHost component(device);
    KRITVA_CHECK(host.add_component(component).has_value());
    runtime::EventLog events;
    host.set_event_sink(&events);
    KRITVA_CHECK(host.configure(runtime::parse_configuration("runtime.name=hw_test\n").value()).has_value());

    // Normal operation through the runtime.
    KRITVA_CHECK(host.initialize().has_value() && host.start().has_value());
    KRITVA_CHECK(accel->lifecycle_state() == LifecycleState::RUNNING && gyro->lifecycle_state() == LifecycleState::RUNNING);
    TestSample s;
    KRITVA_CHECK(accel->read(s).has_value() && gyro->read(s).has_value());
    const auto obs = host.observe().value();
    KRITVA_CHECK(obs.components.size() == 1 && obs.components[0].observation.health.state() == HealthState::HEALTHY);

    // An endpoint failure is observable through the unchanged I2 observation, and shutdown stays possible.
    KRITVA_CHECK(gyro->inject_fault("gyro lost").has_value());
    const auto failures = host.failure_report().value();
    KRITVA_CHECK(failures.any() && failures.failed.size() == 1 && failures.failed[0] == component.info().id());
    KRITVA_CHECK(host.observe().value().components[0].observation.health.detail() == "angular_velocity: gyro lost");
    KRITVA_CHECK(accel->read(s).has_value());                                     // the healthy endpoint keeps working
    KRITVA_CHECK(!gyro->read(s).has_value());                                     // the faulted one refuses
    KRITVA_CHECK(host.controlled_shutdown().has_value());
    KRITVA_CHECK(host.state() == LifecycleState::STOPPED);
    KRITVA_CHECK(accel->lifecycle_state() == LifecycleState::STOPPED && gyro->lifecycle_state() == LifecycleState::STOPPED);
    std::printf("contract_runtime_integration_test: PASS\n");
    return 0;
}
