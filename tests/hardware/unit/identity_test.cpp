//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : identity_test.cpp
// Description : Unit tests of Device and Endpoint identity.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-101; DER-102; DER-202; DER-302
// API         : HARDWARE-IDENTITY-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <set>
#include <string>
#include <type_traits>

#include "../../runtime/check.hpp"

#include <kritva/hardware/identity.hpp>

using namespace kritva::hardware;
using kritva::core::ErrorCode;

static void test_ids_are_strong_and_validated() {
    KRITVA_CHECK(!DeviceId{}.valid() && !DeviceId{0}.valid() && DeviceId{1}.valid());
    KRITVA_CHECK(DeviceId{7}.value() == 7 && DeviceId{1} < DeviceId{2} && DeviceId{3} == DeviceId{3});
    static_assert(!std::is_convertible_v<DeviceId, EndpointId>, "device and endpoint ids must not mix");
    static_assert(!std::is_convertible_v<std::uint64_t, DeviceId>, "ids are constructed explicitly");
    std::set<EndpointId> ordered{EndpointId{3}, EndpointId{1}, EndpointId{2}};   // deterministic ordering
    KRITVA_CHECK(ordered.begin()->value() == 1);
}

static void test_names() {
    KRITVA_CHECK(valid_name("left_arm_imu") && valid_name("a") && valid_name("x9_y") && valid_name(std::string(64, 'a')));
    KRITVA_CHECK(!valid_name("") && !valid_name(std::string(65, 'a')));
    for (const char* bad : {"Imu", "left.arm", "left arm", "a-b", "a\n", "é", "a/b"}) KRITVA_CHECK(!valid_name(bad));
}

static void test_device_info() {
    const auto ok = DeviceInfo::create(DeviceId{1}, "left_arm_imu");
    KRITVA_CHECK(ok.has_value() && ok.value().id() == DeviceId{1} && ok.value().name() == "left_arm_imu");
    auto bad = DeviceInfo::create(DeviceId{}, "x");
    KRITVA_CHECK(!bad.has_value() && bad.error().code == ErrorCode::INVALID_ARGUMENT);
    bad = DeviceInfo::create(DeviceId{1}, "Bad.Name");
    KRITVA_CHECK(!bad.has_value() && bad.error().code == ErrorCode::INVALID_ARGUMENT);
}

static void test_endpoint_info() {
    const auto ok = EndpointInfo::create(EndpointId{2}, "acceleration", EndpointDirection::SENSOR);
    KRITVA_CHECK(ok.has_value() && ok.value().id() == EndpointId{2} && ok.value().name() == "acceleration");
    KRITVA_CHECK(ok.value().direction() == EndpointDirection::SENSOR);
    KRITVA_CHECK(EndpointInfo::create(EndpointId{2}, "command", EndpointDirection::ACTUATOR).value().direction() == EndpointDirection::ACTUATOR);
    KRITVA_CHECK(!EndpointInfo::create(EndpointId{}, "x", EndpointDirection::SENSOR).has_value());
    KRITVA_CHECK(!EndpointInfo::create(EndpointId{1}, "", EndpointDirection::SENSOR).has_value());
}

int main() {
    test_ids_are_strong_and_validated();
    test_names();
    test_device_info();
    test_endpoint_info();
    std::printf("identity_test: PASS\n");
    return 0;
}
