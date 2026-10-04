//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : mock_imu.cpp
// Description : Mock IMU device implementation.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Mock Hardware
// Layer       : Hardware Abstraction
//
// Requirements: DER-802; DER-803; DER-702
// API         : kritva::hardware::mock::MockImuDevice
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <kritva/hardware/mock/mock_imu.hpp>

#include <memory>
#include <variant>

namespace kritva::hardware::mock {

namespace {

EndpointInfo sensor_info(EndpointId id, const char* name) {
    return EndpointInfo::create(id, name, EndpointDirection::SENSOR).value();
}

// `start` and `step` as integers; a missing setting keeps its default.
core::Result<void> read_sequence(const core::Configuration& cfg, std::int64_t& start, std::int64_t& step) {
    for (auto [name, target] : {std::pair<const char*, std::int64_t*>{"start", &start}, {"step", &step}}) {
        if (const core::Parameter* p = cfg.get(name)) {
            const auto* v = std::get_if<std::int64_t>(&p->value);
            if (v == nullptr || *v < -1'000'000 || *v > 1'000'000) {
                return core::Result<void>::failure(core::Error{core::ErrorCode::CONFIGURATION_ERROR, core::ErrorSeverity::ERROR, {}, {},
                    std::string("'") + name + "' must be an integer in [-1000000, 1000000]"});
            }
            *target = *v;
        }
    }
    return core::Result<void>::success();
}

std::vector<std::string> sequence_settings(std::vector<std::string> injection) {
    injection.insert(injection.begin(), {"start", "step"});
    return injection;
}

} // namespace

MockAccelerationEndpoint::MockAccelerationEndpoint(EndpointId id, const char* name)
    : MockEndpoint<AccelerationEndpoint>(sensor_info(id, name), acceleration_capability(), "ops") {}

std::vector<std::string> MockAccelerationEndpoint::setting_names() const { return sequence_settings(injection_settings()); }

core::Result<void> MockAccelerationEndpoint::on_configure(const core::Configuration& scoped) {
    std::int64_t start = 0, step = 1;                       // a configure replaces the previous settings
    if (auto r = read_sequence(scoped, start, step); !r) return r;
    if (auto r = configure_injection(scoped); !r) return r;
    start_ = start;
    step_ = step;
    return core::Result<void>::success();
}

core::Result<void> MockAccelerationEndpoint::on_initialize() {
    produced_ = 0;
    return MockEndpoint<AccelerationEndpoint>::on_initialize();
}

core::Result<void> MockAccelerationEndpoint::do_read(AccelerationSample& out) {
    if (auto r = scheduled_failure("read"); !r) return r;
    ++produced_;
    const double x = static_cast<double>(start_ + step_ * static_cast<std::int64_t>(produced_ - 1));
    out = AccelerationSample{Vec3{x, -x, 9.81}, produced_, virtual_timestamp(produced_)};
    operation_succeeded();
    return core::Result<void>::success();
}

MockAngularVelocityEndpoint::MockAngularVelocityEndpoint(EndpointId id, const char* name)
    : MockEndpoint<AngularVelocityEndpoint>(sensor_info(id, name), angular_velocity_capability(), "ops") {}

std::vector<std::string> MockAngularVelocityEndpoint::setting_names() const { return sequence_settings(injection_settings()); }

core::Result<void> MockAngularVelocityEndpoint::on_configure(const core::Configuration& scoped) {
    std::int64_t start = 0, step = 1;
    if (auto r = read_sequence(scoped, start, step); !r) return r;
    if (auto r = configure_injection(scoped); !r) return r;
    start_ = start;
    step_ = step;
    return core::Result<void>::success();
}

core::Result<void> MockAngularVelocityEndpoint::on_initialize() {
    produced_ = 0;
    return MockEndpoint<AngularVelocityEndpoint>::on_initialize();
}

core::Result<void> MockAngularVelocityEndpoint::do_read(AngularVelocitySample& out) {
    if (auto r = scheduled_failure("read"); !r) return r;
    ++produced_;
    const double x = static_cast<double>(start_ + step_ * static_cast<std::int64_t>(produced_ - 1));
    out = AngularVelocitySample{Vec3{x, 0.0, 0.0}, produced_, virtual_timestamp(produced_)};
    operation_succeeded();
    return core::Result<void>::success();
}

MockImuDevice::MockImuDevice(DeviceInfo info) : Device(std::move(info)) {
    auto accel = std::make_unique<MockAccelerationEndpoint>();
    auto gyro = std::make_unique<MockAngularVelocityEndpoint>();
    acceleration_ = accel.get();
    angular_velocity_ = gyro.get();
    (void)add_endpoint(std::move(accel));                   // fixed, distinct ids and names: cannot fail
    (void)add_endpoint(std::move(gyro));
}

} // namespace kritva::hardware::mock
