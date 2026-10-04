//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : mock_motor.cpp
// Description : Mock motor device implementation.
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

#include <kritva/hardware/mock/mock_motor.hpp>

#include <cmath>
#include <variant>

namespace kritva::hardware::mock {

namespace {

core::Result<std::int64_t> read_int(const core::Configuration& cfg, const char* name, std::int64_t fallback, std::int64_t min, std::int64_t max) {
    const core::Parameter* p = cfg.get(name);
    if (p == nullptr) return core::Result<std::int64_t>::success(fallback);
    const auto* v = std::get_if<std::int64_t>(&p->value);
    if (v == nullptr || *v < min || *v > max) {
        return core::Result<std::int64_t>::failure(core::Error{core::ErrorCode::CONFIGURATION_ERROR, core::ErrorSeverity::ERROR, {}, {},
            std::string("'") + name + "' must be an integer in [" + std::to_string(min) + ", " + std::to_string(max) + "]"});
    }
    return core::Result<std::int64_t>::success(*v);
}

} // namespace

// ---- command ---------------------------------------------------------------------------------

MockMotorCommandEndpoint::MockMotorCommandEndpoint(std::shared_ptr<MotorModel> model, EndpointId id, const char* name)
    : MockEndpoint<MotorCommandEndpoint>(EndpointInfo::create(id, name, EndpointDirection::ACTUATOR).value(), motor_command_capability(), "writes"),
      model_(std::move(model)) {}

std::vector<std::string> MockMotorCommandEndpoint::setting_names() const {
    auto names = injection_settings();
    names.insert(names.begin(), {"min_rad_s", "max_rad_s"});
    return names;
}

core::Result<void> MockMotorCommandEndpoint::set_limits(const MotorLimits& limits) {
    const auto state = lifecycle_state();
    if (state != core::LifecycleState::UNKNOWN && state != core::LifecycleState::STOPPED) {
        return core::Result<void>::failure(make_error(core::ErrorCode::INVALID_STATE, "limits cannot change once the endpoint is operational"));
    }
    if (auto r = validate_limits(limits); !r) return r;
    if (std::fabs(limits.min_rad_s) > kMaxLimitRadS || std::fabs(limits.max_rad_s) > kMaxLimitRadS) {
        return core::Result<void>::failure(make_error(core::ErrorCode::INVALID_ARGUMENT, "motor limits exceed the allowed ceiling"));
    }
    limits_ = limits;
    return core::Result<void>::success();
}

core::Result<void> MockMotorCommandEndpoint::on_configure(const core::Configuration& scoped) {
    // A setting that is absent keeps the current limit exactly (no integer round trip).
    MotorLimits wanted = limits_;
    if (scoped.contains("min_rad_s")) {
        const auto lo = read_int(scoped, "min_rad_s", 0, -1000, 1000);
        if (!lo) return core::Result<void>::failure(lo.error());
        wanted.min_rad_s = static_cast<double>(lo.value());
    }
    if (scoped.contains("max_rad_s")) {
        const auto hi = read_int(scoped, "max_rad_s", 0, -1000, 1000);
        if (!hi) return core::Result<void>::failure(hi.error());
        wanted.max_rad_s = static_cast<double>(hi.value());
    }
    if (auto r = validate_limits(wanted); !r) {
        return core::Result<void>::failure(core::Error{core::ErrorCode::CONFIGURATION_ERROR, core::ErrorSeverity::ERROR, {}, {}, r.error().message});
    }
    if (auto r = configure_injection(scoped); !r) return r;
    limits_ = wanted;
    return core::Result<void>::success();
}

core::Result<void> MockMotorCommandEndpoint::on_initialize() {
    model_->velocity_rad_s = 0.0;                            // never starts moving
    return MockEndpoint<MotorCommandEndpoint>::on_initialize();
}

void MockMotorCommandEndpoint::on_fault() {
    model_->velocity_rad_s = 0.0;                            // a faulted motor is commanded to zero, in the model itself
}

core::Result<void> MockMotorCommandEndpoint::on_stop() {
    model_->velocity_rad_s = 0.0;
    return core::Result<void>::success();
}

core::Result<void> MockMotorCommandEndpoint::on_shutdown() {
    model_->velocity_rad_s = 0.0;
    return core::Result<void>::success();
}

core::Result<void> MockMotorCommandEndpoint::do_write(const MotorCommand& command) {
    if (auto r = validate(command, limits_); !r) return r;   // validate BEFORE acting
    if (auto r = scheduled_failure("write"); !r) return r;   // a failed write is not applied
    model_->velocity_rad_s = command.velocity_rad_s;
    operation_succeeded();
    return core::Result<void>::success();
}

// ---- position --------------------------------------------------------------------------------

MockPositionEndpoint::MockPositionEndpoint(std::shared_ptr<MotorModel> model, const MockMotorCommandEndpoint& command, EndpointId id, const char* name)
    : MockEndpoint<PositionEndpoint>(EndpointInfo::create(id, name, EndpointDirection::SENSOR).value(), position_capability(), "ops"),
      model_(std::move(model)), command_(command) {}

std::vector<std::string> MockPositionEndpoint::setting_names() const {
    auto names = injection_settings();
    names.insert(names.begin(), "initial");
    return names;
}

core::Result<void> MockPositionEndpoint::on_configure(const core::Configuration& scoped) {
    const auto initial = read_int(scoped, "initial", 0, -1'000'000, 1'000'000);
    if (!initial) return core::Result<void>::failure(initial.error());
    if (auto r = configure_injection(scoped); !r) return r;
    initial_ = initial.value();
    return core::Result<void>::success();
}

core::Result<void> MockPositionEndpoint::on_initialize() {
    produced_ = 0;
    model_->position_rad = static_cast<double>(initial_);
    return MockEndpoint<PositionEndpoint>::on_initialize();
}

core::Result<void> MockPositionEndpoint::do_read(PositionSample& out) {
    if (auto r = scheduled_failure("read"); !r) return r;
    model_->position_rad += command_.effective_velocity() * kPositionStepSeconds;   // 0 when the command endpoint is not RUNNING
    ++produced_;
    out = PositionSample{model_->position_rad, produced_, virtual_timestamp(produced_)};
    operation_succeeded();
    return core::Result<void>::success();
}

// ---- device ----------------------------------------------------------------------------------

MockMotorDevice::MockMotorDevice(DeviceInfo info) : Device(std::move(info)), model_(std::make_shared<MotorModel>()) {
    auto command = std::make_unique<MockMotorCommandEndpoint>(model_);
    command_ = command.get();
    auto position = std::make_unique<MockPositionEndpoint>(model_, *command_);
    position_ = position.get();
    (void)add_endpoint(std::move(command));                  // fixed, distinct ids and names: cannot fail
    (void)add_endpoint(std::move(position));
}

} // namespace kritva::hardware::mock
