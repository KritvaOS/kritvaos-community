//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device_demo_test.cpp
// Description : System tests of the reference device demo, run in-process.
//
// Component   : KritvaOS Demo
// Module      : Tests
// Layer       : Application
//
// Requirements: DER-804; DER-805; DER-702; DER-703; DER-704
// API         : DEVICE-DEMO-ST
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <initializer_list>
#include <sstream>
#include <string>

#include "../../runtime/check.hpp"

#include <kritva/runtime/configuration.hpp>

#include "device_demo_app.hpp"

using namespace kritva;
using namespace kritva::demo;

struct Run {
    DeviceDemoOutcome outcome;
    std::string output;
};

static Run run_demo(const char* config_text) {
    std::ostringstream out;
    DeviceDemoApplication app(out);
    const auto cfg = runtime::parse_configuration(config_text, device_demo_config_keys());
    KRITVA_CHECK(cfg.has_value());
    const auto outcome = app.run(cfg.value());
    return {outcome, out.str()};
}

static bool contains(const Run& r, const std::string& t) { return r.output.find(t) != std::string::npos; }

static bool in_order(const Run& r, std::initializer_list<const char*> tokens) {
    std::size_t pos = 0;
    for (const char* t : tokens) {
        const auto idx = r.output.find(t, pos);
        if (idx == std::string::npos) return false;
        pos = idx + std::string(t).size();
    }
    return true;
}

static void test_fault_scenario() {                                  // the complete I3 flow
    const auto r = run_demo("runtime.name=demo\ndemo.ticks=6\n");
    KRITVA_CHECK(r.outcome == DeviceDemoOutcome::COMPLETED && exit_code(r.outcome) == 0);
    KRITVA_CHECK(in_order(r, {"registered devices: 2",
                              "discovered left_arm_imu.acceleration (sensor, id=1) capabilities=acceleration",
                              "discovered left_arm_imu.angular_velocity (sensor, id=2)",
                              "discovered shoulder_motor.command (actuator, id=1) capabilities=motor_command",
                              "discovered shoulder_motor.position (sensor, id=2) capabilities=position",
                              "state=UNKNOWN", "state=READY", "state=RUNNING",
                              "motor command 0.500 rad/s accepted", "tick 1 accel=(0.0,-0.0,9.81) gyro=(0.0,0.0,0.0) position=0.0050",
                              "device left_arm_imu (id=1) enabled=true status=OK health=HEALTHY",
                              "tick 3", "injecting a fault into left_arm_imu.angular_velocity",
                              "FAILURE observed: failed=device_manager",
                              "component device_manager (id=100) state=RUNNING health=UNHEALTHY",
                              "endpoint angular_velocity (id=2) sensor state=FAULT status=FAILED health=UNHEALTHY",
                              "no silent recovery: angular_velocity stays FAULT after 3 further reads",
                              "healthy endpoints still work: acceleration read ok",
                              "tick 4 accel=(4.0,-4.0,9.81) gyro=unavailable", "tick 6",
                              "state=STOPPED", "controlled shutdown after the fault complete (error events=1)"}));
}

static void test_clean_scenario() {
    const auto r = run_demo("runtime.name=demo\ndemo.ticks=4\ndemo.inject_fault=false\n");
    KRITVA_CHECK(r.outcome == DeviceDemoOutcome::COMPLETED);
    KRITVA_CHECK(contains(r, "clean shutdown complete (error events=0)") && !contains(r, "FAILURE") && !contains(r, "injecting"));
    KRITVA_CHECK(contains(r, "tick 4") && contains(r, "device left_arm_imu (id=1) enabled=true status=OK health=HEALTHY"));
    KRITVA_CHECK(!contains(r, "gyro=unavailable"));
}

static void test_invalid_configurations() {
    for (const char* bad : {"demo.ticks=3\n",                                                  // runtime.name is required
                            "runtime.name=demo\ndemo.ticks=0\n",
                            "runtime.name=demo\ndemo.ticks=2000\n",
                            "runtime.name=demo\ndemo.inject_fault=maybe\n",
                            "runtime.name=demo\ndemo.ticks=3\ndemo.fault_after_tick=4\n",       // after the last tick
                            "runtime.name=demo\ndemo.fault_after_tick=0\n",
                            "runtime.name=demo\ndemo.command_milli_rad_s=9999999\n",
                            "runtime.name=demo\nshoulder_motor.command.min_rad_s=3\nshoulder_motor.command.max_rad_s=2\n",
                            "runtime.name=demo\nleft_arm_imu.acceleration.step=x\n"}) {
        const auto r = run_demo(bad);
        KRITVA_CHECK(r.outcome == DeviceDemoOutcome::ERROR && exit_code(r.outcome) == 1);
        KRITVA_CHECK(contains(r, "ERROR") && !contains(r, "state=READY") && !contains(r, "state=RUNNING"));   // never reaches operation
    }
}

static void test_unusable_command_ends_in_a_controlled_error() {
    // 5 rad/s is outside the default [-1, 1] limit of the mock motor: the write is rejected, the demo shuts down.
    const auto r = run_demo("runtime.name=demo\ndemo.command_milli_rad_s=5000\n");
    KRITVA_CHECK(r.outcome == DeviceDemoOutcome::ERROR && contains(r, "WRITE FAILED: motor command is outside the allowed range"));
    KRITVA_CHECK(!contains(r, "injecting a fault"));
}

static void test_disabled_device_is_an_error() {
    const auto r = run_demo("runtime.name=demo\nshoulder_motor.enabled=false\n");
    KRITVA_CHECK(r.outcome == DeviceDemoOutcome::ERROR && contains(r, "WRITE FAILED"));
}

static void test_configured_device_settings_are_active() {
    const auto r = run_demo("runtime.name=demo\nleft_arm_imu.acceleration.start=100\nleft_arm_imu.acceleration.step=10\ndemo.inject_fault=false\n");
    KRITVA_CHECK(r.outcome == DeviceDemoOutcome::COMPLETED && contains(r, "tick 1 accel=(100.0,-100.0,9.81)") && contains(r, "tick 2 accel=(110.0,-110.0,9.81)"));
}

static void test_scheduled_mock_fault_through_configuration() {
    // The mock itself faults after 2 reads (no demo injection): the demo's read then fails and the demo ends in a controlled error.
    const auto r = run_demo("runtime.name=demo\ndemo.inject_fault=false\nleft_arm_imu.acceleration.fault_after_ops=2\n");
    KRITVA_CHECK(r.outcome == DeviceDemoOutcome::ERROR && contains(r, "READ FAILED") && !contains(r, "tick 4"));
}

static void test_allow_list_rejects_typos() {
    const auto typo = runtime::parse_configuration("runtime.name=demo\ndemo.tick=3\n", device_demo_config_keys());
    KRITVA_CHECK(!typo.has_value() && typo.error().message == "line 2: unknown key 'demo.tick'");
    KRITVA_CHECK(!runtime::parse_configuration("runtime.name=demo\nshoulder_motor.command.max_rad=2\n", device_demo_config_keys()).has_value());
}

static void test_deterministic_and_repeatable() {
    const char* text = "runtime.name=demo\ndemo.ticks=8\ndemo.fault_after_tick=5\n";
    const auto first = run_demo(text);
    KRITVA_CHECK(first.output == run_demo(text).output);                                          // identical runs
    for (int i = 0; i < 4; ++i) {                                                                 // no state leaks between runs
        KRITVA_CHECK(run_demo("runtime.name=demo\n").outcome == DeviceDemoOutcome::COMPLETED);
        KRITVA_CHECK(run_demo("runtime.name=demo\ndemo.inject_fault=false\n").outcome == DeviceDemoOutcome::COMPLETED);
    }
}

int main() {
    test_fault_scenario();
    test_clean_scenario();
    test_invalid_configurations();
    test_unusable_command_ends_in_a_controlled_error();
    test_disabled_device_is_an_error();
    test_configured_device_settings_are_active();
    test_scheduled_mock_fault_through_configuration();
    test_allow_list_rejects_typos();
    test_deterministic_and_repeatable();
    std::printf("device_demo_test: PASS\n");
    return 0;
}
