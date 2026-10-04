//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : test_endpoints.hpp
// Description : Minimal test doubles of the Endpoint contracts (test only; not mocks).
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-801; DER-802
// API         : TestEndpoint / TestSensor / TestActuator
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <kritva/hardware/actuator_endpoint.hpp>
#include <kritva/hardware/device.hpp>
#include <kritva/hardware/sensor_endpoint.hpp>

namespace kritva::hardware::test {

inline core::Error injected(const char* message = "injected failure") {
    return core::Error{core::ErrorCode::INTERNAL_ERROR, core::ErrorSeverity::ERROR, {}, {}, message};
}

inline EndpointInfo make_info(std::uint64_t id, const char* name, EndpointDirection d) {
    return EndpointInfo::create(EndpointId{id}, name, d).value();
}

inline core::CapabilitySet one_capability(std::uint64_t id, const char* name) {
    core::CapabilitySet set;
    set.add(core::Capability{core::CapabilityId{id}, name, {}});
    return set;
}

/// Which hook of a TestEndpoint should fail.
enum class FailAt { NONE, CONFIGURE, INITIALIZE, START, STOP, SHUTDOWN };

/// Base double: records hook calls and can fail one hook. Exposes the protected
/// fault/degradation helpers so tests can drive them.
template <class Base>
class Instrumented : public Base {
public:
    using Base::Base;
    FailAt fail_at{FailAt::NONE};
    int configure_calls{0}, initialize_calls{0}, start_calls{0}, stop_calls{0}, shutdown_calls{0};

    core::Result<void> inject_fault(const char* message = "injected fault") { return this->enter_fault(injected(message)); }
    void degrade(const char* detail) { this->set_degraded(detail); }
    void recover_degradation() { this->clear_degraded(); }

protected:
    core::Result<void> on_configure(const core::Configuration&) override { ++configure_calls; return hook(FailAt::CONFIGURE); }
    core::Result<void> on_initialize() override { ++initialize_calls; return hook(FailAt::INITIALIZE); }
    core::Result<void> on_start() override { ++start_calls; return hook(FailAt::START); }
    core::Result<void> on_stop() override { ++stop_calls; return hook(FailAt::STOP); }
    core::Result<void> on_shutdown() override { ++shutdown_calls; return hook(FailAt::SHUTDOWN); }

private:
    core::Result<void> hook(FailAt which) {
        return fail_at == which ? core::Result<void>::failure(injected("hook failure")) : core::Result<void>::success();
    }
};

/// Plain endpoint without data operations.
class TestEndpoint : public Instrumented<Endpoint> {
public:
    explicit TestEndpoint(std::uint64_t id = 1, const char* name = "endpoint", EndpointDirection d = EndpointDirection::SENSOR)
        : Instrumented<Endpoint>(make_info(id, name, d), one_capability(id, name)) {}
};

struct TestSample { std::int32_t value{0}; };
struct TestCommand { std::int32_t value{0}; };

class TestSensor : public Instrumented<SensorEndpoint<TestSample>> {
public:
    explicit TestSensor(std::uint64_t id = 1, const char* name = "sensor")
        : Instrumented<SensorEndpoint<TestSample>>(make_info(id, name, EndpointDirection::SENSOR), one_capability(id, name)) {}
    bool fail_next_read{false};
    int reads{0};

protected:
    core::Result<void> do_read(TestSample& out) override {
        ++reads;
        if (fail_next_read) { fail_next_read = false; return core::Result<void>::failure(injected("read failure")); }
        out.value = reads;                                           // deterministic: 1, 2, 3 ...
        return core::Result<void>::success();
    }
};

class TestActuator : public Instrumented<ActuatorEndpoint<TestCommand>> {
public:
    explicit TestActuator(std::uint64_t id = 1, const char* name = "actuator")
        : Instrumented<ActuatorEndpoint<TestCommand>>(make_info(id, name, EndpointDirection::ACTUATOR), one_capability(id, name)) {}
    std::optional<TestCommand> applied;
    int limit{100};

protected:
    core::Result<void> do_write(const TestCommand& command) override {
        if (command.value > limit || command.value < -limit) {       // validate before acting
            return core::Result<void>::failure(make_error(core::ErrorCode::INVALID_ARGUMENT, "command out of range"));
        }
        applied = command;
        return core::Result<void>::success();
    }
};

} // namespace kritva::hardware::test
