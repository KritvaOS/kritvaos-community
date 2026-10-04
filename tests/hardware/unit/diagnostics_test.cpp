//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : diagnostics_test.cpp
// Description : Unit tests of device and endpoint diagnostics.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-601..608; DER-701; DER-702
// API         : HARDWARE-DIAGNOSTICS-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <memory>
#include <string>

#include "../../runtime/check.hpp"
#include "../support/test_endpoints.hpp"

#include <kritva/hardware/device_manager.hpp>

using namespace kritva::hardware;
using namespace kritva::hardware::test;
using kritva::core::ErrorCode;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
using kritva::core::StatusCode;

struct Rig {
    Device imu{DeviceInfo::create(DeviceId{1}, "imu").value()};
    Device motor{DeviceInfo::create(DeviceId{2}, "motor").value()};
    TestSensor* gyro;
    TestActuator* command;
    DeviceManager manager{kritva::core::runtime::ComponentId{100}};
    Rig() {
        gyro = static_cast<TestSensor*>(imu.add_endpoint(std::make_unique<TestSensor>(1, "gyro")).value());
        command = static_cast<TestActuator*>(motor.add_endpoint(std::make_unique<TestActuator>(1, "command")).value());
        KRITVA_CHECK(manager.register_device(imu).has_value() && manager.register_device(motor).has_value());
    }
};

static void test_snapshot_before_use() {                             // DER-601, DER-602
    Rig r;
    const auto d = r.manager.diagnostics();
    KRITVA_CHECK(d.size() == 2 && d[0].name == "imu" && d[1].name == "motor" && d[0].id == DeviceId{1});
    KRITVA_CHECK(d[0].enabled && d[0].status == StatusCode::UNKNOWN && d[0].health == HealthState::UNKNOWN);
    const auto& e = d[0].endpoints.at(0);
    KRITVA_CHECK(e.device_id == DeviceId{1} && e.device_name == "imu" && e.endpoint_id == EndpointId{1} && e.endpoint_name == "gyro");
    KRITVA_CHECK(e.direction == EndpointDirection::SENSOR && d[1].endpoints.at(0).direction == EndpointDirection::ACTUATOR);
    KRITVA_CHECK(e.lifecycle == LifecycleState::UNKNOWN && e.operations_ok == 0 && e.operations_failed == 0 && !e.last_error && e.health_detail.empty());
}

static void test_running_snapshot() {                                // DER-603..606
    Rig r;
    KRITVA_CHECK(r.manager.initialize().has_value() && r.manager.start().has_value());
    TestSample s;
    KRITVA_CHECK(r.gyro->read(s).has_value() && r.gyro->read(s).has_value());
    KRITVA_CHECK(r.command->write(TestCommand{1}).has_value() && !r.command->write(TestCommand{9999}).has_value());
    const auto d = r.manager.diagnostics();
    KRITVA_CHECK(d[0].status == StatusCode::OK && d[0].health == HealthState::HEALTHY);
    KRITVA_CHECK(d[0].endpoints[0].lifecycle == LifecycleState::RUNNING && d[0].endpoints[0].operations_ok == 2);
    KRITVA_CHECK(d[1].endpoints[0].operations_ok == 1 && d[1].endpoints[0].operations_failed == 1);
    KRITVA_CHECK(d[1].endpoints[0].last_error && d[1].endpoints[0].last_error->code == ErrorCode::INVALID_ARGUMENT);   // last failure is observable
    KRITVA_CHECK(d[0].capabilities == std::vector<std::string>{"gyro"} && d[1].endpoints[0].capabilities == std::vector<std::string>{"command"});
}

static void test_fault_evidence() {                                  // DER-607, DER-701, DER-703
    Rig r;
    KRITVA_CHECK(r.manager.initialize().has_value() && r.manager.start().has_value());
    KRITVA_CHECK(r.gyro->inject_fault("gyro lost").has_value());
    auto d = r.manager.diagnostics();
    const auto& e = d[0].endpoints[0];
    KRITVA_CHECK(e.lifecycle == LifecycleState::FAULT && e.status == StatusCode::FAILED && e.health == HealthState::UNHEALTHY);
    KRITVA_CHECK(e.health_detail == "gyro lost");                                    // the deterministic failure reason
    KRITVA_CHECK(e.last_error && e.last_error->message == "gyro lost" && e.last_error->code == ErrorCode::INTERNAL_ERROR);
    KRITVA_CHECK(d[0].health == HealthState::UNHEALTHY && d[0].health_detail == "gyro: gyro lost" && d[0].status == StatusCode::FAILED);
    KRITVA_CHECK(d[1].health == HealthState::HEALTHY);                               // the other device is unaffected

    TestSample s;
    for (int i = 0; i < 3; ++i) KRITVA_CHECK(!r.gyro->read(s).has_value());           // refused reads are counted and recorded
    d = r.manager.diagnostics();
    KRITVA_CHECK(d[0].endpoints[0].operations_failed == 3 && d[0].endpoints[0].last_error->code == ErrorCode::RESOURCE_UNAVAILABLE);
    KRITVA_CHECK(d[0].endpoints[0].lifecycle == LifecycleState::FAULT);              // still faulted: no silent recovery
}

static void test_disabled_device_is_marked() {
    Rig r;
    kritva::core::Configuration cfg;
    KRITVA_CHECK(cfg.set(kritva::core::Parameter{"motor.enabled", false, {}}).has_value());
    KRITVA_CHECK(r.manager.configure(cfg).has_value());
    const auto d = r.manager.diagnostics();
    KRITVA_CHECK(d[0].enabled && !d[1].enabled && !d[1].endpoints[0].enabled);
}

static void test_text_rendering_is_exact_and_deterministic() {       // DER-702
    Rig r;
    const std::string before = describe(r.manager.diagnostics());
    KRITVA_CHECK(before == describe(r.manager.diagnostics()));
    const std::string expected_before =
        "device imu (id=1) enabled=true status=UNKNOWN health=UNKNOWN capabilities=gyro\n"
        "  endpoint gyro (id=1) sensor state=UNKNOWN status=UNKNOWN health=UNKNOWN ok=0 failed=0 capabilities=gyro\n"
        "device motor (id=2) enabled=true status=UNKNOWN health=UNKNOWN capabilities=command\n"
        "  endpoint command (id=1) actuator state=UNKNOWN status=UNKNOWN health=UNKNOWN ok=0 failed=0 capabilities=command\n";
    KRITVA_CHECK(before == expected_before);

    KRITVA_CHECK(r.manager.initialize().has_value() && r.manager.start().has_value());
    TestSample s;
    KRITVA_CHECK(r.gyro->read(s).has_value() && r.gyro->inject_fault("gyro lost").has_value());
    const std::string after = describe(r.manager.diagnostics());
    KRITVA_CHECK(after.find("device imu (id=1) enabled=true status=FAILED health=UNHEALTHY detail=\"gyro: gyro lost\"") != std::string::npos);
    KRITVA_CHECK(after.find("  endpoint gyro (id=1) sensor state=FAULT status=FAILED health=UNHEALTHY detail=\"gyro lost\" ok=1 failed=0 "
                            "capabilities=gyro last_error=INTERNAL_ERROR \"gyro lost\"\n") != std::string::npos);
}

static void test_messages_cannot_forge_lines() {                     // output integrity, DER-608
    Rig r;
    KRITVA_CHECK(r.manager.initialize().has_value() && r.manager.start().has_value());
    KRITVA_CHECK(r.gyro->inject_fault("lost\ndevice evil (id=9) health=HEALTHY\r\tx").has_value());
    const std::string text = describe(r.manager.diagnostics());
    KRITVA_CHECK(text.find("\ndevice evil") == std::string::npos);                    // never at the start of a line
    std::size_t lines = 0;
    for (char c : text) if (c == '\n') ++lines;
    KRITVA_CHECK(lines == 4);                                                         // exactly two devices and two endpoints
    KRITVA_CHECK(text.find('\r') == std::string::npos && text.find('\t') == std::string::npos);
}

static void test_quotes_cannot_forge_fields() {                       // m4
    Device d(DeviceInfo::create(DeviceId{1}, "d").value());
    auto* e = static_cast<TestSensor*>(d.add_endpoint(std::make_unique<TestSensor>(1, "e")).value());
    DeviceManager manager{kritva::core::runtime::ComponentId{100}};
    KRITVA_CHECK(manager.register_device(d).has_value());
    KRITVA_CHECK(manager.initialize().has_value() && manager.start().has_value());
    KRITVA_CHECK(e->inject_fault("x\" health=HEALTHY ok=999 \"y").has_value());
    const std::string text = describe(manager.diagnostics());

    // Every quoted field is delimited by exactly its own two quotes: device detail (2), endpoint detail (2) and last_error (2).
    std::size_t quotes = 0;
    for (char c : text) if (c == '"') ++quotes;
    KRITVA_CHECK(quotes == 6);
    KRITVA_CHECK(text.find("detail=\"e: x? health=HEALTHY ok=999 ?y\"") != std::string::npos);   // defused in place, still inside the field
    KRITVA_CHECK(text.find("last_error=INTERNAL_ERROR \"x? health=HEALTHY ok=999 ?y\"") != std::string::npos);
    std::size_t lines = 0;
    for (char c : text) if (c == '\n') ++lines;
    KRITVA_CHECK(lines == 2);                                                    // one device line and one endpoint line
}

static void test_capability_names_are_sanitised() {                    // m4
    Device d(DeviceInfo::create(DeviceId{1}, "d").value());
    kritva::core::CapabilitySet caps;
    caps.add(kritva::core::Capability{kritva::core::CapabilityId{1}, "ok\ndevice evil (id=9) health=HEALTHY", {}});
    caps.add(kritva::core::Capability{kritva::core::CapabilityId{2}, "a,b\"c", {}});
    struct Carrier final : Endpoint {
        Carrier(kritva::core::CapabilitySet c) : Endpoint(make_info(1, "carrier", EndpointDirection::SENSOR), std::move(c)) {}
    };
    KRITVA_CHECK(d.add_endpoint(std::make_unique<Carrier>(caps)).has_value());
    DeviceManager manager{kritva::core::runtime::ComponentId{100}};
    KRITVA_CHECK(manager.register_device(d).has_value());
    const std::string text = describe(manager.diagnostics());
    KRITVA_CHECK(text.find("\ndevice evil") == std::string::npos);
    std::size_t lines = 0;
    for (char c : text) if (c == '\n') ++lines;
    KRITVA_CHECK(lines == 2);                                                    // one device line, one endpoint line
    KRITVA_CHECK(text.find("capabilities=ok?device?evil?(id=9)?health=HEALTHY,a?b?c") != std::string::npos);   // no spaces: no forged tokens
    KRITVA_CHECK(text.find("health=HEALTHY,a") != std::string::npos && text.find(" health=HEALTHY,") == std::string::npos);
}

static void test_diagnostics_never_contain_configuration_values() {   // DER-608: regression guard
    Rig r;
    kritva::core::Configuration cfg;
    KRITVA_CHECK(cfg.set(kritva::core::Parameter{"imu.gyro.limit", std::int64_t{424242}, {}}).has_value());
    KRITVA_CHECK(cfg.set(kritva::core::Parameter{"imu.enabled", true, {}}).has_value());
    KRITVA_CHECK(cfg.set(kritva::core::Parameter{"imu.gyro.note", std::string("s3cr3t-token"), {}}).has_value());
    KRITVA_CHECK(r.manager.configure(cfg).has_value());
    KRITVA_CHECK(r.manager.initialize().has_value() && r.manager.start().has_value());
    KRITVA_CHECK(r.gyro->inject_fault().has_value());                              // a fault adds health detail and last_error too
    const std::string text = describe(r.manager.diagnostics());
    KRITVA_CHECK(text.find("424242") == std::string::npos && text.find("s3cr3t") == std::string::npos);
}

int main() {
    test_snapshot_before_use();
    test_running_snapshot();
    test_fault_evidence();
    test_disabled_device_is_marked();
    test_text_rendering_is_exact_and_deterministic();
    test_messages_cannot_forge_lines();
    test_quotes_cannot_forge_fields();
    test_capability_names_are_sanitised();
    test_diagnostics_never_contain_configuration_values();
    std::printf("diagnostics_test: PASS\n");
    return 0;
}
