//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : runtime_host_integration_test.cpp
// Description : Integration of RuntimeHost with Core RuntimeManager ordering and multiple components.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-LIF-004; RR-CMP-004; RR-CMP-005
// API         : RUNTIME-HOST-IT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <string>
#include <vector>

#include "../check.hpp"
#include "../probe_component.hpp"

using namespace kritva;
using kritva::core::LifecycleState;
using kritva::runtime::RuntimeHost;
using kritva::runtime::test::ProbeComponent;

// Host + Core registry/dependency graph + components: start in dependency
// order, stop in reverse, runtime reaches RUNNING only after all start.
int main() {
    RuntimeHost host;
    std::vector<std::string> log;
    ProbeComponent a(1, "a", host.runtime(), log);   // a depends on b
    ProbeComponent b(2, "b", host.runtime(), log);
    KRITVA_CHECK(host.runtime().register_component(a).has_value());
    KRITVA_CHECK(host.runtime().register_component(b).has_value());
    KRITVA_CHECK(host.runtime().add_dependency(a.info().id(), b.info().id()).has_value());

    KRITVA_CHECK(host.run().has_value());
    KRITVA_CHECK(host.state() == LifecycleState::STOPPED);

    const std::vector<std::string> expected{
        "b.initialize:INITIALIZING", "a.initialize:INITIALIZING",
        "b.start:READY",             "a.start:READY",
        "a.stop:STOPPING",           "b.stop:STOPPING",
        "a.shutdown:STOPPED",        "b.shutdown:STOPPED"};
    KRITVA_CHECK(log == expected);
    std::printf("runtime_host_integration_test: PASS\n");
    return 0;
}
