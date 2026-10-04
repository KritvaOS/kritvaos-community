//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device_demo_app.cpp
// Description : Reference Device/Endpoint application implementation.
//
// Component   : KritvaOS Demo
// Module      : Device Demo
// Layer       : Application
//
// Requirements: DER-804; DER-401..407; DER-607; DER-703; DER-704
// API         : kritva::demo::DeviceDemoApplication
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include "device_demo_app.hpp"

#include <cstdio>
#include <string>

#include <kritva/hardware/device_manager.hpp>
#include <kritva/hardware/mock/mock_imu.hpp>
#include <kritva/hardware/mock/mock_motor.hpp>
#include <kritva/runtime/runtime_host.hpp>

namespace kritva::demo {

namespace {

using namespace kritva::hardware;
using namespace kritva::hardware::mock;
using runtime::to_string;

constexpr core::runtime::ComponentId kManagerId{100};

std::string fixed(double v, int digits) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", digits, v);
    return buffer;
}

} // namespace

std::vector<std::string> device_demo_config_keys() {
    MockImuDevice imu(DeviceInfo::create(DeviceId{1}, "left_arm_imu").value());
    MockMotorDevice motor(DeviceInfo::create(DeviceId{2}, "shoulder_motor").value());
    DeviceManager manager{kManagerId};
    (void)manager.register_device(imu);
    (void)manager.register_device(motor);
    auto keys = manager.config_keys();
    for (const char* k : {"runtime.name", "demo.ticks", "demo.inject_fault", "demo.fault_after_tick", "demo.command_milli_rad_s"}) keys.push_back(k);
    return keys;
}

DeviceDemoOutcome DeviceDemoApplication::run(const core::Configuration& configuration) {
    const auto say = [&](const std::string& line) { out_ << "[device_demo] " << line << "\n"; };
    const auto fail = [&](const std::string& what, const std::string& message) {
        say(what + ": " + message);
        return DeviceDemoOutcome::ERROR;
    };

    // Demo-level settings, validated before anything is constructed.
    const auto ticks = runtime::get_int(configuration, "demo.ticks", 5, 1, 1000);
    const auto inject = runtime::get_bool(configuration, "demo.inject_fault", true);
    if (!ticks) return fail("CONFIG ERROR", ticks.error().message);
    if (!inject) return fail("CONFIG ERROR", inject.error().message);
    const auto fault_tick = runtime::get_int(configuration, "demo.fault_after_tick", 3, 1, ticks.value());
    const auto command_milli = runtime::get_int(configuration, "demo.command_milli_rad_s", 500, -1'000'000, 1'000'000);
    if (!fault_tick) return fail("CONFIG ERROR", fault_tick.error().message);
    if (!command_milli) return fail("CONFIG ERROR", command_milli.error().message);

    // Devices are declared before the manager and the host: they must outlive the manager.
    MockImuDevice imu(DeviceInfo::create(DeviceId{1}, "left_arm_imu").value());
    MockMotorDevice motor(DeviceInfo::create(DeviceId{2}, "shoulder_motor").value());
    DeviceManager manager{kManagerId};
    runtime::EventLog events;
    runtime::RuntimeHost host;
    host.set_event_sink(&events);
    manager.set_event_sink(&events);

    if (auto r = host.add_component(manager); !r) return fail("COMPOSE ERROR", r.error().message);
    if (auto r = manager.register_device(imu); !r) return fail("REGISTER ERROR", r.error().message);
    if (auto r = manager.register_device(motor); !r) return fail("REGISTER ERROR", r.error().message);
    if (auto r = host.configure(configuration); !r) return fail("CONFIG ERROR", r.error().message);
    say("config: " + runtime::describe_configuration(host.settings(), configuration.size()));
    say("registered devices: " + std::to_string(manager.registry().size()));

    // Discovery: every endpoint is found by device and endpoint name and used through its typed interface.
    const auto& registry = manager.registry();
    struct Found { AccelerationEndpoint* accel; AngularVelocityEndpoint* gyro; MotorCommandEndpoint* command; PositionEndpoint* position; } found{};
    const auto discover = [&](const char* device, const char* endpoint) -> Endpoint* {
        const auto e = registry.find_endpoint(device, endpoint);
        if (!e) { say(std::string("DISCOVERY ERROR: ") + e.error().message); return nullptr; }
        std::string caps;
        for (const auto& c : e.value()->capabilities().all()) caps += (caps.empty() ? "" : ",") + c.name;
        say(std::string("discovered ") + device + "." + endpoint + " (" +
            (e.value()->info().direction() == EndpointDirection::SENSOR ? "sensor" : "actuator") +
            ", id=" + std::to_string(e.value()->info().id().value()) + ") capabilities=" + caps);
        return e.value();
    };
    Endpoint* a = discover("left_arm_imu", "acceleration");
    Endpoint* g = discover("left_arm_imu", "angular_velocity");
    Endpoint* c = discover("shoulder_motor", "command");
    Endpoint* p = discover("shoulder_motor", "position");
    if (a == nullptr || g == nullptr || c == nullptr || p == nullptr) return DeviceDemoOutcome::ERROR;
    found.accel = dynamic_cast<AccelerationEndpoint*>(a);
    found.gyro = dynamic_cast<AngularVelocityEndpoint*>(g);
    found.command = dynamic_cast<MotorCommandEndpoint*>(c);
    found.position = dynamic_cast<PositionEndpoint*>(p);
    if (found.accel == nullptr || found.gyro == nullptr || found.command == nullptr || found.position == nullptr) {
        return fail("DISCOVERY ERROR", "an endpoint does not have the expected type");
    }

    const auto shutdown_with = [&](const std::string& what, const std::string& message) {
        (void)host.controlled_shutdown();
        return fail(what, message);
    };

    say(std::string("state=") + std::string(to_string(host.state())));
    if (auto r = host.initialize(); !r) return shutdown_with("INITIALIZE FAILED", r.error().message);
    say(std::string("state=") + std::string(to_string(host.state())));
    if (auto r = host.start(); !r) return shutdown_with("START FAILED", r.error().message);
    say(std::string("state=") + std::string(to_string(host.state())));

    // Deterministic activity.
    bool faulted = false;
    for (std::int64_t tick = 1; tick <= ticks.value(); ++tick) {
        AccelerationSample accel;
        PositionSample position;
        if (auto r = found.accel->read(accel); !r) return shutdown_with("READ FAILED", r.error().message);
        if (tick == 1) {
            const MotorCommand command{static_cast<double>(command_milli.value()) / 1000.0};
            if (auto r = found.command->write(command); !r) return shutdown_with("WRITE FAILED", r.error().message);
            say("motor command " + fixed(command.velocity_rad_s, 3) + " rad/s accepted");
        }
        if (auto r = found.position->read(position); !r) return shutdown_with("READ FAILED", r.error().message);
        std::string gyro_text = "unavailable";
        if (!faulted) {
            AngularVelocitySample gyro;
            if (auto r = found.gyro->read(gyro); !r) return shutdown_with("READ FAILED", r.error().message);
            gyro_text = "(" + fixed(gyro.value.x, 1) + "," + fixed(gyro.value.y, 1) + "," + fixed(gyro.value.z, 1) + ")";
        }
        say("tick " + std::to_string(tick) + " accel=(" + fixed(accel.value.x, 1) + "," + fixed(accel.value.y, 1) + "," + fixed(accel.value.z, 2) +
            ") gyro=" + gyro_text + " position=" + fixed(position.value, 4));

        if (tick == 1) {                                         // status, health, capabilities and statistics while healthy
            out_ << describe(manager.diagnostics());
        }
        if (inject.value() && tick == fault_tick.value()) {
            say("injecting a fault into left_arm_imu.angular_velocity");
            if (auto r = imu.angular_velocity().inject_fault("injected demo fault"); !r) return shutdown_with("FAULT INJECTION FAILED", r.error().message);
            faulted = true;

            // The failure must be observed: component level (I2 observation) and endpoint level (diagnostics).
            const auto report = host.failure_report();
            if (!report || !report.value().any() || report.value().failed.front() != manager.info().id()) {
                return shutdown_with("EXPECTATION FAILED", "the endpoint failure was not observed by the runtime");
            }
            say("FAILURE observed: failed=device_manager");
            const auto observation = host.observe({{manager.info().id(), &manager}});
            if (observation) out_ << runtime::describe(observation.value());
            out_ << describe(manager.diagnostics());

            // Healthy endpoints keep working; the faulted one refuses and does not recover.
            AngularVelocitySample gyro;
            for (int attempt = 1; attempt <= 3; ++attempt) {
                const auto r = found.gyro->read(gyro);
                if (r.has_value() || imu.angular_velocity().lifecycle_state() != core::LifecycleState::FAULT) {
                    return shutdown_with("EXPECTATION FAILED", "the faulted endpoint recovered by itself");
                }
            }
            say("no silent recovery: angular_velocity stays FAULT after 3 further reads");
            AccelerationSample still;
            if (auto r = found.accel->read(still); !r) return shutdown_with("EXPECTATION FAILED", "a healthy endpoint stopped working");
            say("healthy endpoints still work: acceleration read ok");
        }
    }

    // Final status of everything, then the controlled shutdown.
    if (!inject.value()) out_ << describe(manager.diagnostics());
    if (auto r = host.controlled_shutdown(); !r) return fail("SHUTDOWN FAILED", r.error().message);
    say(std::string("state=") + std::string(to_string(host.state())));
    if (host.state() != core::LifecycleState::STOPPED || motor.model().velocity_rad_s != 0.0) {
        return fail("EXPECTATION FAILED", "the runtime did not stop cleanly");
    }
    int errors = 0;
    for (const auto& e : events.snapshot()) {
        if (e.type == core::EventType::ERROR && e.source_id == manager.info().id()) ++errors;
    }
    say(std::string(inject.value() ? "controlled shutdown after the fault" : "clean shutdown") + " complete (error events=" + std::to_string(errors) + ")");
    if (errors != (inject.value() ? 1 : 0)) return fail("EXPECTATION FAILED", "unexpected number of error events");
    return DeviceDemoOutcome::COMPLETED;
}

} // namespace kritva::demo
