//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : demo_application_test.cpp
// Description : System tests of the reference application, run in-process.
//
// Component   : KritvaOS Demo
// Module      : Tests
// Layer       : Application
//
// Requirements: RR-APP-001..006; RR-TST-006; RR-TST-007; RR-PERF-003
// API         : DEMO-APPLICATION-IT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <sstream>
#include <string>

#include "../check.hpp"

#include <kritva/runtime/configuration.hpp>

#include "demo_app.hpp"

using namespace kritva;
using namespace kritva::demo;

struct Run {
    DemoOutcome outcome;
    std::string output;
};

static Run run_demo(const char* config_text) {
    std::ostringstream out;
    DemoApplication app(out);
    const auto cfg = runtime::parse_configuration(config_text);
    KRITVA_CHECK(cfg.has_value());
    const auto outcome = app.run(cfg.value());
    return {outcome, out.str()};
}

static bool contains(const Run& r, const std::string& text) { return r.output.find(text) != std::string::npos; }

// Tokens must appear in this order.
static bool in_order(const Run& r, std::initializer_list<const char*> tokens) {
    std::size_t pos = 0;
    for (const char* t : tokens) {
        const auto idx = r.output.find(t, pos);
        if (idx == std::string::npos) return false;
        pos = idx;
    }
    return true;
}

static void test_normal_run() {
    const auto r = run_demo("runtime.name=demo\ndemo.ticks=3\n");
    KRITVA_CHECK(r.outcome == DemoOutcome::CLEAN && exit_code(r.outcome) == 0);
    KRITVA_CHECK(in_order(r, {"composed: sensor controller monitor", "state=UNKNOWN", "state=READY", "state=RUNNING",
                              "tick 1 sensor=10 command=20 failed=0", "tick 2", "tick 3 sensor=30 command=60",
                              "component sensor (id=1) state=RUNNING health=HEALTHY", "state=STOPPED", "shutdown complete"}));
    KRITVA_CHECK(contains(r, "samples=3 errors=0"));                 // statistics observable
    KRITVA_CHECK(!contains(r, "FAILURE"));
}

static void test_failure_run() {
    const auto r = run_demo("runtime.name=demo\ndemo.ticks=10\nsensor.failure_after_ticks=3\n");
    KRITVA_CHECK(r.outcome == DemoOutcome::FAILURE_HANDLED && exit_code(r.outcome) == 3);
    KRITVA_CHECK(in_order(r, {"state=RUNNING", "tick 3 sensor=30", "component sensor (id=1) state=FAULT health=UNHEALTHY",
                              "FAILURE observed: failed=sensor affected=controller,monitor", "event ERROR source=1",
                              "state=STOPPED", "controlled shutdown complete"}));
    KRITVA_CHECK(!contains(r, "tick 4"));                            // the loop ended at the failure
    KRITVA_CHECK(!contains(r, "shutdown complete (events") || contains(r, "controlled shutdown complete"));
}

static void test_failure_run_other_gain_and_ticks() {
    const auto r = run_demo("runtime.name=demo\ndemo.ticks=4\nsensor.failure_after_ticks=1\ncontroller.gain=5\n");
    KRITVA_CHECK(r.outcome == DemoOutcome::FAILURE_HANDLED && contains(r, "tick 1 sensor=10"));
}

static void test_failure_beyond_run_length_is_clean() {
    const auto r = run_demo("runtime.name=demo\ndemo.ticks=2\nsensor.failure_after_ticks=3\n");
    KRITVA_CHECK(r.outcome == DemoOutcome::CLEAN);
}

static void test_invalid_configuration() {
    for (const char* bad : {"runtime.name=demo\ncontroller.gain=0\n", "runtime.name=demo\ndemo.ticks=0\n",
                            "runtime.name=demo\nsensor.enabled=maybe\n", "demo.ticks=3\n",
                            "runtime.name=demo\nsensor.failure_after_ticks=-5\n"}) {
        const auto r = run_demo(bad);
        KRITVA_CHECK(r.outcome == DemoOutcome::ERROR && exit_code(r.outcome) == 1);
        KRITVA_CHECK(contains(r, "CONFIG ERROR"));
        KRITVA_CHECK(!contains(r, "state=READY") && !contains(r, "state=RUNNING"));   // never reaches operation
    }
}

static void test_missing_dependency() {
    const auto r = run_demo("runtime.name=demo\nsensor.enabled=false\n");   // controller needs the sensor
    KRITVA_CHECK(r.outcome == DemoOutcome::ERROR);
    // Core validates the topology as soon as configuration is applied: refused before any lifecycle step.
    KRITVA_CHECK(contains(r, "CONFIG ERROR") && !contains(r, "state=READY") && !contains(r, "state=UNKNOWN"));
}

static void test_optional_components() {
    const auto r = run_demo("runtime.name=demo\nmonitor.enabled=false\ncontroller.enabled=false\ndemo.ticks=2\n");
    KRITVA_CHECK(r.outcome == DemoOutcome::CLEAN && contains(r, "composed: sensor\n"));
}

static void test_deterministic_output() {                            // RR-PERF-003
    const char* text = "runtime.name=demo\ndemo.ticks=10\nsensor.failure_after_ticks=4\n";
    KRITVA_CHECK(run_demo(text).output == run_demo(text).output);
    KRITVA_CHECK(run_demo("runtime.name=demo\n").output == run_demo("runtime.name=demo\n").output);
}

static void test_repeatable_in_one_process() {                       // RR-REL-003: no state leaks between runs
    for (int i = 0; i < 5; ++i) {
        KRITVA_CHECK(run_demo("runtime.name=demo\n").outcome == DemoOutcome::CLEAN);
        KRITVA_CHECK(run_demo("runtime.name=demo\nsensor.failure_after_ticks=2\n").outcome == DemoOutcome::FAILURE_HANDLED);
    }
}

int main() {
    test_normal_run();
    test_failure_run();
    test_failure_run_other_gain_and_ticks();
    test_failure_beyond_run_length_is_clean();
    test_invalid_configuration();
    test_missing_dependency();
    test_optional_components();
    test_deterministic_output();
    test_repeatable_in_one_process();
    std::printf("demo_application_test: PASS\n");
    return 0;
}
