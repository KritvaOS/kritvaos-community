//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : component_composition_integration_test.cpp
// Description : Integration: multi-component composition with a mid-sequence failure.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-CMP-004; RR-CMP-005; RR-DEP-004; RR-FLT-006
// API         : RUNTIME-COMPOSITION-IT
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
using kritva::runtime::test::ProbeComponent;
using Log = std::vector<std::string>;

// a depends on b, b depends on c. If b fails to start, a never starts, the
// failure is returned unchanged with b as its source, and the controlled
// cleanup releases every component, ending STOPPED.
int main() {
    RuntimeHost host;
    Log log;
    ProbeComponent c(3, "c", host.runtime(), log);
    ProbeComponent b(2, "b", host.runtime(), log, ProbeComponent::Hook::START);
    ProbeComponent a(1, "a", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a, {b.info().id()}).has_value());
    KRITVA_CHECK(host.add_component(b, {c.info().id()}).has_value());
    KRITVA_CHECK(host.add_component(c).has_value());

    const auto r = host.run();
    KRITVA_CHECK(!r.has_value());
    KRITVA_CHECK(r.error().code == ErrorCode::INTERNAL_ERROR && r.error().source == b.info().id());
    KRITVA_CHECK(host.state() == LifecycleState::STOPPED);

    // Core reset(): components that were live (a initialized, c started) are stopped in reverse
    // order; b faulted in start() and is only shut down; shutdown runs in reverse order.
    // a never starts. The runtime is still FAULT while these hooks run (it reaches STOPPED after).
    const Log expected{"c.initialize:INITIALIZING", "b.initialize:INITIALIZING", "a.initialize:INITIALIZING",
                       "c.start:READY", "b.start:READY",
                       "a.stop:FAULT", "c.stop:FAULT",
                       "a.shutdown:FAULT", "b.shutdown:FAULT", "c.shutdown:FAULT"};
    KRITVA_CHECK(log == expected);
    std::printf("component_composition_integration_test: PASS\n");
    return 0;
}
