//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : runtime_host_test.cpp
// Description : Unit tests of RuntimeHost: lifecycle, invalid transitions, shutdown.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-LIF-001..008; RR-REL-003
// API         : RUNTIME-HOST-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <string>
#include <vector>

#include "../check.hpp"
#include "../probe_component.hpp"

using namespace kritva;
using kritva::core::ErrorCode;
using kritva::core::LifecycleState;
using kritva::runtime::RuntimeHost;
using kritva::runtime::to_string;
using kritva::runtime::test::ProbeComponent;

static void test_initial_state() {                                   // UT-001
    RuntimeHost host;
    KRITVA_CHECK(host.state() == LifecycleState::UNKNOWN);
}

static void test_valid_lifecycle_with_transient_states() {           // UT-002..006
    RuntimeHost host;
    std::vector<std::string> log;
    ProbeComponent probe(1, "probe", host.runtime(), log);
    KRITVA_CHECK(host.runtime().register_component(probe).has_value());

    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.initialize().has_value());
    KRITVA_CHECK(host.state() == LifecycleState::READY);
    KRITVA_CHECK(host.start().has_value());
    KRITVA_CHECK(host.state() == LifecycleState::RUNNING);
    KRITVA_CHECK(host.stop().has_value());
    KRITVA_CHECK(host.state() == LifecycleState::STOPPED);
    KRITVA_CHECK(host.shutdown().has_value());
    KRITVA_CHECK(host.state() == LifecycleState::STOPPED);

    // Core is INITIALIZING while components initialize, and STOPPING while they stop.
    const std::vector<std::string> expected{
        "probe.initialize:INITIALIZING", "probe.start:READY", "probe.stop:STOPPING", "probe.shutdown:STOPPED"};
    KRITVA_CHECK(log == expected);
}

static void test_invalid_transitions_rejected() {                    // UT-007
    RuntimeHost host;
    auto r = host.start();                                           // start before initialize
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_STATE);
    r = host.stop();
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_STATE);
    KRITVA_CHECK(host.state() == LifecycleState::UNKNOWN);

    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.initialize().has_value());
    r = host.initialize();                                           // double initialize
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_STATE);
    KRITVA_CHECK(host.state() == LifecycleState::READY);

    KRITVA_CHECK(host.start().has_value());
    r = host.start();                                                // double start
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_STATE);
    r = host.shutdown();                                             // shutdown while RUNNING
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_STATE);
    KRITVA_CHECK(host.state() == LifecycleState::RUNNING);

    KRITVA_CHECK(host.stop().has_value());
    r = host.stop();                                                 // stop when already STOPPED
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_STATE);
    KRITVA_CHECK(host.state() == LifecycleState::STOPPED);
}

static void test_initialize_without_configuration_fails() {         // RR-CFG-003
    RuntimeHost host;
    std::vector<std::string> log;
    ProbeComponent probe(1, "probe", host.runtime(), log);
    KRITVA_CHECK(host.runtime().register_component(probe).has_value());
    const auto r = host.initialize();
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::CONFIGURATION_ERROR);
    KRITVA_CHECK(host.state() == LifecycleState::UNKNOWN && log.empty());      // nothing invoked
    KRITVA_CHECK(!host.run().has_value() && host.state() == LifecycleState::UNKNOWN && log.empty());

    // An invalid configuration does not satisfy the requirement either.
    KRITVA_CHECK(!host.configure(kritva::runtime::parse_configuration("runtime.tick_ms=5\n").value()).has_value());
    KRITVA_CHECK(!host.initialize().has_value());

    // A valid one does, and the requirement is sticky across restarts.
    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.initialize().has_value() && host.state() == LifecycleState::READY);
    KRITVA_CHECK(host.stop().has_value());
    KRITVA_CHECK(host.initialize().has_value());
}

static void test_run_reports_observable_states() {
    RuntimeHost host;
    std::vector<LifecycleState> seen;
    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.run([&](LifecycleState s) { seen.push_back(s); }).has_value());
    const std::vector<LifecycleState> expected{LifecycleState::READY, LifecycleState::RUNNING, LifecycleState::STOPPED};
    KRITVA_CHECK(seen == expected);
    KRITVA_CHECK(host.state() == LifecycleState::STOPPED);
    KRITVA_CHECK(host.run().has_value());                            // run() without observer is valid
}

static void test_repeated_lifecycle() {                              // RR-REL-003
    RuntimeHost host;
    std::vector<std::string> log;
    ProbeComponent probe(1, "probe", host.runtime(), log);
    KRITVA_CHECK(host.runtime().register_component(probe).has_value());
    for (int i = 0; i < 3; ++i) {
        kritva::runtime::test::configure_host(host);
        KRITVA_CHECK(host.run().has_value());
        KRITVA_CHECK(host.state() == LifecycleState::STOPPED);
    }
    KRITVA_CHECK(log.size() == 12);                                  // 4 hooks x 3 cycles, nothing stuck
}

static void test_failed_run_cleans_up() {                            // RR-FLT-006
    for (auto hook : {ProbeComponent::Hook::INITIALIZE, ProbeComponent::Hook::START, ProbeComponent::Hook::STOP}) {
        RuntimeHost host;
        std::vector<std::string> log;
        ProbeComponent probe(1, "probe", host.runtime(), log, hook);
        KRITVA_CHECK(host.runtime().register_component(probe).has_value());
        kritva::runtime::test::configure_host(host);
        const auto r = host.run();
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INTERNAL_ERROR);   // original error kept
        KRITVA_CHECK(host.state() == LifecycleState::STOPPED);       // controlled cleanup, not stuck in FAULT
        KRITVA_CHECK(host.runtime().fault_error() == nullptr);
    }
}

static void test_state_names() {
    KRITVA_CHECK(to_string(LifecycleState::INITIALIZING) == "INITIALIZING");
    KRITVA_CHECK(to_string(LifecycleState::STOPPING) == "STOPPING");
    KRITVA_CHECK(to_string(LifecycleState::FAULT) == "FAULT");
}

int main() {
    test_initial_state();
    test_valid_lifecycle_with_transient_states();
    test_invalid_transitions_rejected();
    test_initialize_without_configuration_fails();
    test_run_reports_observable_states();
    test_repeated_lifecycle();
    test_failed_run_cleans_up();
    test_state_names();
    std::printf("runtime_host_test: PASS\n");
    return 0;
}
