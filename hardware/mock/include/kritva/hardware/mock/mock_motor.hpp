//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : mock_motor.hpp
// Description : Mock motor device: a velocity command endpoint and a position endpoint that integrates it.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Mock Hardware
// Layer       : Hardware Abstraction
//
// Requirements: DER-802; DER-803; DER-502; DER-702
// API         : kritva::hardware::mock::MockMotorDevice
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <memory>

#include <kritva/hardware/mock/mock_endpoint.hpp>

namespace kritva::hardware::mock {

/// The simulated joint shared by the two endpoints of a MockMotorDevice.
struct MotorModel {
    double velocity_rad_s{0.0};
    double position_rad{0.0};
};

/// Duration, in seconds, that one position read advances the simulated joint.
inline constexpr double kPositionStepSeconds = 0.01;

/// Mock velocity command sink. Settings: `min_rad_s` / `max_rad_s` (int rad/s, default
/// -1 / 1; min <= max, |v| <= 1000), `fail_after_writes`, `fault_after_writes`. Fractional
/// limits can be set with set_limits().
///
/// A write is validated BEFORE it is applied (NaN, Inf and out-of-range commands are rejected
/// with INVALID_ARGUMENT and change nothing). A scheduled failure fails the write without
/// applying it. FAIL-SAFE: the simulated joint velocity is zero whenever this endpoint is not
/// RUNNING, and is reset to zero by initialize(), stop() and shutdown().
class MockMotorCommandEndpoint : public MockEndpoint<MotorCommandEndpoint> {
public:
    MockMotorCommandEndpoint(std::shared_ptr<MotorModel> model, EndpointId id = EndpointId{1}, const char* name = "command");
    [[nodiscard]] std::vector<std::string> setting_names() const override;
    core::Result<void> set_limits(const MotorLimits& limits);
    [[nodiscard]] const MotorLimits& limits() const noexcept { return limits_; }
    /// Velocity the joint currently has (0 unless a command is applied and the endpoint is RUNNING).
    [[nodiscard]] double effective_velocity() const noexcept {
        return lifecycle_state() == core::LifecycleState::RUNNING ? model_->velocity_rad_s : 0.0;
    }

protected:
    core::Result<void> on_configure(const core::Configuration& scoped) override;
    core::Result<void> on_initialize() override;
    core::Result<void> on_stop() override;
    core::Result<void> on_shutdown() override;
    core::Result<void> do_write(const MotorCommand& command) override;

private:
    std::shared_ptr<MotorModel> model_;
    MotorLimits limits_{};
};

/// Mock joint position sensor. Settings: `initial` (int rad, default 0), `fail_after_ops`,
/// `fault_after_ops`. Each read advances the joint by effective velocity x kPositionStepSeconds
/// (the velocity of the paired command endpoint, 0 when it is not RUNNING) and reports the new
/// position; sample n has sequence n and timestamp virtual_timestamp(n).
class MockPositionEndpoint : public MockEndpoint<PositionEndpoint> {
public:
    MockPositionEndpoint(std::shared_ptr<MotorModel> model, const MockMotorCommandEndpoint& command,
                         EndpointId id = EndpointId{2}, const char* name = "position");
    [[nodiscard]] std::vector<std::string> setting_names() const override;

protected:
    core::Result<void> on_configure(const core::Configuration& scoped) override;
    core::Result<void> on_initialize() override;
    core::Result<void> do_read(PositionSample& out) override;

private:
    std::shared_ptr<MotorModel> model_;
    const MockMotorCommandEndpoint& command_;
    std::int64_t initial_{0};
    std::uint64_t produced_{0};
};

/// A simulated motor: endpoints `command` (id 1) and `position` (id 2) over one MotorModel.
class MockMotorDevice final : public Device {
public:
    explicit MockMotorDevice(DeviceInfo info);
    [[nodiscard]] MockMotorCommandEndpoint& command() noexcept { return *command_; }
    [[nodiscard]] MockPositionEndpoint& position() noexcept { return *position_; }
    [[nodiscard]] const MotorModel& model() const noexcept { return *model_; }

private:
    std::shared_ptr<MotorModel> model_;
    MockMotorCommandEndpoint* command_;
    MockPositionEndpoint* position_;
};

} // namespace kritva::hardware::mock
