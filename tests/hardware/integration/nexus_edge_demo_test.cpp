//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : nexus_edge_demo_test.cpp
// Description : System test of the KOS-I4 reference application: the full scenario, determinism, the clean variant and the
//               rejection of invalid or misspelled configuration before anything runs.
//
// Component   : KritvaOS Examples
// Module      : Tests
// Layer       : Application
//
// Requirements: VR-001; VR-002; VR-003 (HUMAN SAFETY REVIEW OPEN)
// API         : NEXUS-EDGE-DEMO-IT
//
// Author      : KritvaOS
// Created     : 07-10-2026
//==============================================================================

#include <initializer_list>
#include <sstream>
#include <string>

#include "../../runtime/check.hpp"

#include <kritva/runtime/configuration.hpp>

#include "nexus_edge_demo_app.hpp"

using namespace kritva;
using namespace kritva::demo;

struct Run {
    NexusEdgeDemoOutcome outcome;
    std::string output;
};

static Run run_demo(const std::string& config_text) {
    std::ostringstream out;
    NexusEdgeDemoApplication app(out);
    const auto cfg = runtime::parse_configuration(config_text, nexus_edge_demo_config_keys());
    if (!cfg) return {NexusEdgeDemoOutcome::ERROR, "loader: " + cfg.error().message};
    return {app.run(cfg.value()), out.str()};
}

static bool in_order(const std::string& out, std::initializer_list<const char*> tokens) {
    std::size_t pos = 0;
    for (const char* t : tokens) {
        const auto at = out.find(t, pos);
        if (at == std::string::npos) { std::fprintf(stderr, "missing or out of order: %s\n", t); return false; }
        pos = at + std::string(t).size();
    }
    return true;
}

static const char* kDefault = "runtime.name=demo\n";

static void test_the_full_scenario_completes() {
    const Run r = run_demo(kDefault);
    KRITVA_CHECK(r.outcome == NexusEdgeDemoOutcome::COMPLETED && exit_code(r.outcome) == 0);
    KRITVA_CHECK(in_order(r.output, {
        "HELLO accepted, discovery complete: 2 remote devices", "discovered shoulder_motor.command (actuator", "registered devices: 2",
        "state=READY", "state=RUNNING", "each sensor delivered its first sample", "motor command 0.500 rad/s accepted by the Edge",
        "out-of-limit command is refused by the Edge's own validation", "runtime observation:", "device diagnostics (the existing I3 path):",
        "both sides kept sending heartbeats", "hostile link: duplicated and replayed frames", "a duplicated write was applied once",
        "the replayed older write was dropped as stale and never applied", "injecting link loss", "the Nexus knows", "one ERROR event per endpoint (4)",
        "the Edge has not timed out yet", "heartbeat timeout expired: it stopped its own actuator", "the Edge's sensors keep running",
        "Nexus diagnostics after the loss:", "fault_origin=LINK_LOST", "Edge diagnostics after the loss:", "UNHEALTHY through the existing observation path",
        "no reconnect, nothing sent", "recovery: stop, shutdown, initialize, start", "a fresh session 3 (was 2)", "the second live period works",
        "heartbeats flow again", "everything is STOPPED, on the Nexus and on the Edge", "controlled shutdown complete (error events=4)", "RESULT: PASS"}));
    KRITVA_CHECK(r.output.find("EXPECTATION FAILED") == std::string::npos && r.output.find("ERROR:") == std::string::npos);
}

static void test_the_run_is_deterministic() {                                  // no wall clock, no threads
    const Run a = run_demo(kDefault);
    const Run b = run_demo(kDefault);
    KRITVA_CHECK(a.output == b.output && !a.output.empty());
    // A different link gives a different (but equally reproducible) transcript.
    const std::string other = "runtime.name=demo\nlink.heartbeat_period_ms=50\nlink.heartbeat_timeout_ms=200\nlink.request_timeout_ms=50\ndemo.link_latency_ms=3\n";
    const Run c = run_demo(other);
    KRITVA_CHECK(c.outcome == NexusEdgeDemoOutcome::COMPLETED && c.output != a.output && c.output == run_demo(other).output);
}

static void test_the_configured_timing_governs_the_scenario() {
    const Run r = run_demo("runtime.name=demo\nlink.heartbeat_period_ms=50\nlink.heartbeat_timeout_ms=200\nlink.request_timeout_ms=50\n");
    KRITVA_CHECK(r.outcome == NexusEdgeDemoOutcome::COMPLETED);
    KRITVA_CHECK(r.output.find("link timing: heartbeat 50 ms, timeout 200 ms, request timeout 50 ms") != std::string::npos);
    KRITVA_CHECK(r.output.find("heartbeats over 500 ms: Nexus sent 10, Edge sent 10") != std::string::npos);
}

static void test_the_clean_variant_has_no_hostile_traffic() {
    const Run r = run_demo("runtime.name=demo\ndemo.hostile_link=false\ndemo.link_latency_ms=0\ndemo.run_ms=300\ndemo.command_milli_rad_s=750\n");
    KRITVA_CHECK(r.outcome == NexusEdgeDemoOutcome::COMPLETED);
    KRITVA_CHECK(r.output.find("hostile link") == std::string::npos && r.output.find("motor command 0.750 rad/s accepted") != std::string::npos);
    KRITVA_CHECK(in_order(r.output, {"injecting link loss", "heartbeat timeout expired", "a fresh session", "RESULT: PASS"}));
}

static void test_the_edge_limits_are_the_edges_own() {
    const Run r = run_demo("runtime.name=demo\nshoulder_motor.command.max_rad_s=1\nshoulder_motor.command.min_rad_s=-1\ndemo.command_milli_rad_s=800\n");   // the hostile phase replays 1.2 x this: still inside the limit
    KRITVA_CHECK(r.outcome == NexusEdgeDemoOutcome::COMPLETED && r.output.find("motor command 0.800 rad/s accepted by the Edge") != std::string::npos);
    // A command beyond the Edge's configured limit makes the demo's own step fail, which ends the scenario with an error.
    const Run bad = run_demo("runtime.name=demo\nshoulder_motor.command.max_rad_s=1\ndemo.command_milli_rad_s=1500\n");
    KRITVA_CHECK(bad.outcome == NexusEdgeDemoOutcome::ERROR && bad.output.find("WRITE FAILED") != std::string::npos && bad.output.find("RESULT: PASS") == std::string::npos);
}

static void test_invalid_configuration_is_rejected_before_anything_runs() {
    for (const char* text : {
             "demo.run_ms=500\n",                                                           // runtime.name is required by the runtime
             "runtime.name=demo\nlink.heartbeat_period_ms=200\nlink.heartbeat_timeout_ms=300\n",   // timeout below two periods
             "runtime.name=demo\nlink.request_timeout_ms=0\n",
             "runtime.name=demo\ndemo.run_ms=5\n",                                          // out of range
             "runtime.name=demo\ndemo.link_latency_ms=51\n",
             "runtime.name=demo\ndemo.hostile_link=maybe\n",
             "runtime.name=demo\nlink.heartbeat_period_ms=fast\n"}) {
        const Run r = run_demo(text);
        KRITVA_CHECK(r.outcome == NexusEdgeDemoOutcome::ERROR && exit_code(r.outcome) == 1);
        KRITVA_CHECK(r.output.find("state=READY") == std::string::npos && r.output.find("RESULT: PASS") == std::string::npos);
    }
    // A bad link timing or demo value is refused before a single frame is sent: no HELLO, no discovery.
    const Run timing = run_demo("runtime.name=demo\nlink.heartbeat_period_ms=200\nlink.heartbeat_timeout_ms=300\n");
    KRITVA_CHECK(timing.output.find("CONFIG ERROR") != std::string::npos && timing.output.find("HELLO accepted") == std::string::npos);
}

static void test_a_misspelled_key_is_reported_by_the_loader() {
    const Run r = run_demo("runtime.name=demo\nshoulder_motor.command.max_rad=2\n");
    KRITVA_CHECK(r.outcome == NexusEdgeDemoOutcome::ERROR && r.output.find("unknown key 'shoulder_motor.command.max_rad'") != std::string::npos);
    const Run typo = run_demo("runtime.name=demo\nlink.heartbeat_period=100\n");
    KRITVA_CHECK(typo.outcome == NexusEdgeDemoOutcome::ERROR && typo.output.find("unknown key 'link.heartbeat_period'") != std::string::npos);
}

static void test_the_allow_list_covers_every_documented_key() {
    const auto keys = nexus_edge_demo_config_keys();
    const auto has = [&](const char* k) { for (const auto& x : keys) if (x == k) return true; return false; };
    for (const char* k : {"runtime.name", "link.heartbeat_period_ms", "link.heartbeat_timeout_ms", "link.request_timeout_ms", "link.pump_quantum_ms", "demo.link_latency_ms",
                          "demo.command_milli_rad_s", "demo.run_ms", "demo.hostile_link", "left_arm_imu.enabled", "shoulder_motor.enabled", "left_arm_imu.acceleration.start",
                          "left_arm_imu.angular_velocity.step", "shoulder_motor.command.min_rad_s", "shoulder_motor.command.max_rad_s", "shoulder_motor.position.initial"}) {
        KRITVA_CHECK(has(k));
    }
}

int main() {
    test_the_full_scenario_completes();
    test_the_run_is_deterministic();
    test_the_configured_timing_governs_the_scenario();
    test_the_clean_variant_has_no_hostile_traffic();
    test_the_edge_limits_are_the_edges_own();
    test_invalid_configuration_is_rejected_before_anything_runs();
    test_a_misspelled_key_is_reported_by_the_loader();
    test_the_allow_list_covers_every_documented_key();
    std::printf("nexus_edge_demo_test: PASS\n");
    return 0;
}
