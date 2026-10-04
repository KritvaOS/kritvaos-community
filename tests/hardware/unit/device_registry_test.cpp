//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device_registry_test.cpp
// Description : Unit tests of the Device registry.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-101..108
// API         : HARDWARE-REGISTRY-UT
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

static std::unique_ptr<Device> make_device(std::uint64_t id, const char* name, std::initializer_list<const char*> endpoints = {}) {
    auto d = std::make_unique<Device>(DeviceInfo::create(DeviceId{id}, name).value());
    std::uint64_t eid = 1;
    for (const char* e : endpoints) KRITVA_CHECK(d->add_endpoint(std::make_unique<TestSensor>(eid++, e)).has_value());
    return d;
}

static void test_empty_registry() {
    DeviceRegistry r;
    KRITVA_CHECK(r.size() == 0 && r.devices().empty() && !r.closed());
    const auto d = r.find(DeviceId{1});
    KRITVA_CHECK(!d.has_value() && d.error().code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(!r.find("imu").has_value() && !r.find_endpoint("imu", "accel").has_value());
}

static void test_register_and_find() {                               // DER-105
    auto imu = make_device(10, "left_arm_imu", {"acceleration", "angular_velocity"});
    auto motor = make_device(20, "shoulder_motor", {"command", "position"});
    DeviceRegistry r;
    KRITVA_CHECK(r.register_device(*imu).has_value() && r.register_device(*motor).has_value());
    KRITVA_CHECK(r.size() == 2);
    KRITVA_CHECK(r.find(DeviceId{10}).value() == imu.get() && r.find("shoulder_motor").value() == motor.get());
    KRITVA_CHECK(r.find(DeviceId{20}).value() == motor.get() && r.find("left_arm_imu").value() == imu.get());
}

static void test_endpoint_lookup() {                                 // DER-106
    auto imu = make_device(10, "left_arm_imu", {"acceleration", "angular_velocity"});
    auto motor = make_device(20, "shoulder_motor", {"command", "position"});
    DeviceRegistry r;
    KRITVA_CHECK(r.register_device(*imu).has_value() && r.register_device(*motor).has_value());
    // The same endpoint id (1) exists in both devices: the Device id disambiguates.
    KRITVA_CHECK(r.find_endpoint(DeviceId{10}, EndpointId{1}).value() == imu->find_endpoint("acceleration"));
    KRITVA_CHECK(r.find_endpoint(DeviceId{20}, EndpointId{1}).value() == motor->find_endpoint("command"));
    KRITVA_CHECK(r.find_endpoint("shoulder_motor", "position").value() == motor->find_endpoint(EndpointId{2}));
    KRITVA_CHECK(r.find_endpoint("left_arm_imu", "angular_velocity").value() == imu->find_endpoint(EndpointId{2}));
}

static void test_missing_lookups_fail_explicitly() {                 // DER-107
    auto imu = make_device(10, "left_arm_imu", {"acceleration"});
    DeviceRegistry r;
    KRITVA_CHECK(r.register_device(*imu).has_value());
    auto d = r.find(DeviceId{99});
    KRITVA_CHECK(!d.has_value() && d.error().code == ErrorCode::INVALID_ARGUMENT && d.error().message == "no device with id 99");
    d = r.find("nope");
    KRITVA_CHECK(!d.has_value() && d.error().message == "no device named 'nope'");
    auto e = r.find_endpoint(DeviceId{10}, EndpointId{9});
    KRITVA_CHECK(!e.has_value() && e.error().message == "device 'left_arm_imu' has no endpoint with id 9");
    e = r.find_endpoint("left_arm_imu", "nope");
    KRITVA_CHECK(!e.has_value() && e.error().message == "device 'left_arm_imu' has no endpoint named 'nope'");
    e = r.find_endpoint("nope", "acceleration");
    KRITVA_CHECK(!e.has_value() && e.error().message == "no device named 'nope'");
    e = r.find_endpoint(DeviceId{99}, EndpointId{1});
    KRITVA_CHECK(!e.has_value() && e.error().code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(r.size() == 1);                                                   // errors change nothing
}

static void test_duplicates_rejected() {                             // DER-103
    auto a = make_device(1, "alpha");
    auto same_id = make_device(1, "other");
    auto same_name = make_device(2, "alpha");
    DeviceRegistry r;
    KRITVA_CHECK(r.register_device(*a).has_value());
    auto res = r.register_device(*same_id);
    KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::INVALID_ARGUMENT && res.error().message == "duplicate device id");
    res = r.register_device(*same_name);
    KRITVA_CHECK(!res.has_value() && res.error().message == "duplicate device name 'alpha'");
    res = r.register_device(*a);                                                   // the same Device twice
    KRITVA_CHECK(!res.has_value());
    KRITVA_CHECK(r.size() == 1 && r.find(DeviceId{1}).value() == a.get());
}

static void test_duplicate_endpoint_ids_are_rejected_by_the_device() {   // DER-104
    auto d = make_device(1, "alpha", {"one"});
    const auto r = d->add_endpoint(std::make_unique<TestSensor>(1, "two"));
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
    DeviceRegistry reg;
    KRITVA_CHECK(reg.register_device(*d).has_value());
    KRITVA_CHECK(reg.find_endpoint(DeviceId{1}, EndpointId{1}).has_value());
}

static void test_enumeration_is_registration_order() {               // DER-108
    auto c = make_device(30, "charlie");
    auto a = make_device(10, "alpha");
    auto b = make_device(20, "bravo");
    DeviceRegistry r;
    for (Device* d : {c.get(), a.get(), b.get()}) KRITVA_CHECK(r.register_device(*d).has_value());
    KRITVA_CHECK(r.devices().size() == 3 && r.devices()[0] == c.get() && r.devices()[1] == a.get() && r.devices()[2] == b.get());
    // Identical on every run and independent of ids or names.
    DeviceRegistry again;
    for (Device* d : {c.get(), a.get(), b.get()}) KRITVA_CHECK(again.register_device(*d).has_value());
    KRITVA_CHECK(again.devices() == r.devices());
}

static void test_closed_registry() {                                 // DER-206
    auto a = make_device(1, "alpha");
    auto b = make_device(2, "bravo");
    DeviceRegistry r;
    KRITVA_CHECK(r.register_device(*a).has_value());
    r.close();
    r.close();                                                                      // idempotent
    KRITVA_CHECK(r.closed());
    const auto res = r.register_device(*b);
    KRITVA_CHECK(!res.has_value() && res.error().code == ErrorCode::INVALID_STATE && r.size() == 1);
    KRITVA_CHECK(r.find("alpha").has_value());                                      // lookups still work
}

int main() {
    test_empty_registry();
    test_register_and_find();
    test_endpoint_lookup();
    test_missing_lookups_fail_explicitly();
    test_duplicates_rejected();
    test_duplicate_endpoint_ids_are_rejected_by_the_device();
    test_enumeration_is_registration_order();
    test_closed_registry();
    std::printf("device_registry_test: PASS\n");
    return 0;
}
