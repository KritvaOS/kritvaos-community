//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : observation_sanity.cpp
// Description : Sanity executable: prints runtime state, component state, health, statistics and events.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-OBS-001..006
// API         : RUNTIME-OBSERVATION-SANITY
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <cstdio>
#include <string>
#include <vector>

#include "../check.hpp"
#include "../probe_component.hpp"

using namespace kritva;
using kritva::runtime::test::ProbeComponent;

int main() {
    runtime::RuntimeHost host;
    runtime::EventLog events;
    host.set_event_sink(&events);
    std::vector<std::string> log;
    ProbeComponent a(1, "a", host.runtime(), log);
    ProbeComponent b(2, "b", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a, {b.info().id()}).has_value());
    KRITVA_CHECK(host.add_component(b).has_value());

    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.run([&](core::LifecycleState) {
        std::printf("[observe] %s", runtime::describe(host.observe().value()).c_str());
        std::printf("[observe] events=%zu\n", events.size());
    }).has_value());
    return 0;
}
