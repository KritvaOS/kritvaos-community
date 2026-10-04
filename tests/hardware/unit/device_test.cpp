//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device_test.cpp
// Description : Unit tests of the Device contract: ownership, endpoint set, aggregation.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-201..206; DER-105; DER-106; DER-601; DER-603; DER-605
// API         : HARDWARE-DEVICE-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <memory>
#include <string>

#include "../../runtime/check.hpp"
#include "../support/test_endpoints.hpp"

using namespace kritva::hardware;
using namespace kritva::hardware::test;
using kritva::core::ErrorCode;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
using kritva::core::StatusCode;

static int g_destroyed = 0;
struct CountedSensor : TestSensor {
    using TestSensor::TestSensor;
    ~CountedSensor() override { ++g_destroyed; }
};

static Device make_device() { return Device(DeviceInfo::create(DeviceId{1}, "left_arm_imu").value()); }

static TestSensor* add_sensor(Device& d, std::uint64_t id, const char* name) {
    auto r = d.add_endpoint(std::make_unique<TestSensor>(id, name));
    KRITVA_CHECK(r.has_value());
    return static_cast<TestSensor*>(r.value());
}

static void test_identity_and_empty_device() {                       // DER-202
    Device d = make_device();
    KRITVA_CHECK(d.info().id() == DeviceId{1} && d.info().name() == "left_arm_imu");
    KRITVA_CHECK(d.endpoints().empty() && d.find_endpoint(EndpointId{1}) == nullptr && d.find_endpoint("x") == nullptr);
    KRITVA_CHECK(d.status().code() == StatusCode::UNKNOWN && d.health().state() == HealthState::UNKNOWN && d.capabilities().empty());
}

static void test_add_find_and_order() {                              // DER-105, DER-106, DER-203
    Device d = make_device();
    auto* c = add_sensor(d, 3, "charlie");
    auto* a = add_sensor(d, 1, "alpha");
    auto* b = add_sensor(d, 2, "bravo");
    KRITVA_CHECK(d.endpoints().size() == 3);
    KRITVA_CHECK(d.endpoints()[0] == c && d.endpoints()[1] == a && d.endpoints()[2] == b);   // insertion order, deterministic
    KRITVA_CHECK(d.find_endpoint(EndpointId{1}) == a && d.find_endpoint("bravo") == b && d.find_endpoint(EndpointId{3}) == c);
    KRITVA_CHECK(d.find_endpoint(EndpointId{9}) == nullptr && d.find_endpoint("missing") == nullptr);
}

static void test_add_rejections() {                                  // DER-104
    Device d = make_device();
    add_sensor(d, 1, "alpha");
    auto r = d.add_endpoint(std::make_unique<TestSensor>(1, "other"));            // duplicate id
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
    r = d.add_endpoint(std::make_unique<TestSensor>(2, "alpha"));                 // duplicate name
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
    r = d.add_endpoint(nullptr);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(d.endpoints().size() == 1);                                      // nothing was added
}

static void test_endpoint_set_is_fixed_after_use() {                 // DER-206
    Device d = make_device();
    auto* a = add_sensor(d, 1, "alpha");
    auto used = std::make_unique<TestSensor>(5, "used");
    KRITVA_CHECK(used->initialize().has_value());
    auto r = d.add_endpoint(std::move(used));                                     // a used endpoint cannot be added
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_STATE);
    KRITVA_CHECK(a->initialize().has_value());
    r = d.add_endpoint(std::make_unique<TestSensor>(2, "late"));                  // the device was used
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_STATE);
    KRITVA_CHECK(d.endpoints().size() == 1);
}

static void test_device_owns_its_endpoints() {                       // DER-206
    g_destroyed = 0;
    {
        Device d = make_device();
        KRITVA_CHECK(d.add_endpoint(std::make_unique<CountedSensor>(1, "a")).has_value());
        KRITVA_CHECK(d.add_endpoint(std::make_unique<CountedSensor>(2, "b")).has_value());
        KRITVA_CHECK(g_destroyed == 0);
        auto rejected = d.add_endpoint(std::make_unique<CountedSensor>(1, "dup"));   // rejected endpoint is released, not leaked
        KRITVA_CHECK(!rejected.has_value() && g_destroyed == 1);
    }
    KRITVA_CHECK(g_destroyed == 3);                                               // the device destroyed the rest
}

static void test_aggregated_status_and_health() {                    // DER-601, DER-603
    Device d = make_device();
    auto* a = add_sensor(d, 1, "accel");
    auto* g = add_sensor(d, 2, "gyro");
    KRITVA_CHECK(d.status().code() == StatusCode::UNKNOWN && d.health().state() == HealthState::UNKNOWN);

    for (auto* e : {a, g}) KRITVA_CHECK(e->initialize().has_value());
    KRITVA_CHECK(d.status().code() == StatusCode::OK && d.health().state() == HealthState::UNKNOWN);   // READY is not HEALTHY
    for (auto* e : {a, g}) KRITVA_CHECK(e->start().has_value());
    KRITVA_CHECK(d.health().state() == HealthState::HEALTHY);

    g->degrade("noisy");
    KRITVA_CHECK(d.health().state() == HealthState::DEGRADED && d.health().detail() == "gyro: noisy");

    KRITVA_CHECK(a->inject_fault("accel lost").has_value());                       // UNHEALTHY wins over DEGRADED
    KRITVA_CHECK(d.health().state() == HealthState::UNHEALTHY && d.health().detail() == "accel: accel lost");
    KRITVA_CHECK(d.status().code() == StatusCode::FAILED);
    KRITVA_CHECK(g->lifecycle_state() == LifecycleState::RUNNING);                 // the other endpoint is unaffected
}

static void test_aggregated_capabilities() {                         // DER-204, DER-605
    Device d = make_device();
    add_sensor(d, 1, "accel");
    add_sensor(d, 2, "gyro");
    const auto caps = d.capabilities();
    KRITVA_CHECK(caps.size() == 2 && caps.contains(kritva::core::CapabilityId{1}) && caps.contains(kritva::core::CapabilityId{2}));
}

int main() {
    test_identity_and_empty_device();
    test_add_find_and_order();
    test_add_rejections();
    test_endpoint_set_is_fixed_after_use();
    test_device_owns_its_endpoints();
    test_aggregated_status_and_health();
    test_aggregated_capabilities();
    std::printf("device_test: PASS\n");
    return 0;
}
