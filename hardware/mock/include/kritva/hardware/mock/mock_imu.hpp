//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : mock_imu.hpp
// Description : Mock IMU device: deterministic acceleration and angular-velocity endpoints.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Mock Hardware
// Layer       : Hardware Abstraction
//
// Requirements: DER-802; DER-803; DER-702; DER-501
// API         : kritva::hardware::mock::MockImuDevice
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <kritva/hardware/mock/mock_endpoint.hpp>

namespace kritva::hardware::mock {

/// Deterministic accelerometer. Settings: `start` (int, default 0), `step` (int, default 1),
/// plus the failure schedule (`fail_after_ops`, `fault_after_ops`).
/// Sample n (1-based): value = {start + step*(n-1), -(start + step*(n-1)), 9.81} m/s^2,
/// sequence n, timestamp virtual_timestamp(n). n restarts at 1 with each live period.
class MockAccelerationEndpoint : public MockEndpoint<AccelerationEndpoint> {
public:
    explicit MockAccelerationEndpoint(EndpointId id = EndpointId{1}, const char* name = "acceleration");
    [[nodiscard]] std::vector<std::string> setting_names() const override;

protected:
    core::Result<void> on_configure(const core::Configuration& scoped) override;
    core::Result<void> on_initialize() override;
    core::Result<void> do_read(AccelerationSample& out) override;

private:
    std::int64_t start_{0};
    std::int64_t step_{1};
    std::uint64_t produced_{0};
};

/// Deterministic gyroscope. Same settings. Sample n: value = {start + step*(n-1), 0, 0} rad/s.
class MockAngularVelocityEndpoint : public MockEndpoint<AngularVelocityEndpoint> {
public:
    explicit MockAngularVelocityEndpoint(EndpointId id = EndpointId{2}, const char* name = "angular_velocity");
    [[nodiscard]] std::vector<std::string> setting_names() const override;

protected:
    core::Result<void> on_configure(const core::Configuration& scoped) override;
    core::Result<void> on_initialize() override;
    core::Result<void> do_read(AngularVelocitySample& out) override;

private:
    std::int64_t start_{0};
    std::int64_t step_{1};
    std::uint64_t produced_{0};
};

/// A simulated IMU: endpoints `acceleration` (id 1) and `angular_velocity` (id 2).
class MockImuDevice final : public Device {
public:
    explicit MockImuDevice(DeviceInfo info);
    [[nodiscard]] MockAccelerationEndpoint& acceleration() noexcept { return *acceleration_; }
    [[nodiscard]] MockAngularVelocityEndpoint& angular_velocity() noexcept { return *angular_velocity_; }

private:
    MockAccelerationEndpoint* acceleration_;
    MockAngularVelocityEndpoint* angular_velocity_;
};

} // namespace kritva::hardware::mock
