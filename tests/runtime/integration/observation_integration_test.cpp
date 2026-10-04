//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : observation_integration_test.cpp
// Description : Integration: observation stays consistent with the runtime through a full lifecycle.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-OBS-005; RR-OBS-007
// API         : RUNTIME-OBSERVATION-IT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <string>
#include <vector>

#include "../check.hpp"
#include "../probe_component.hpp"

using namespace kritva;
using kritva::core::EventType;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
using kritva::runtime::EventLog;
using kritva::runtime::RuntimeHost;
using kritva::runtime::test::ProbeComponent;

// At each observable lifecycle step the snapshot taken by an independent
// observer must agree with the state the host reports and with the components'
// own accessors, and every step must have produced exactly one event.
int main() {
    RuntimeHost host;
    EventLog events;
    host.set_event_sink(&events);
    std::vector<std::string> log;
    ProbeComponent a(1, "a", host.runtime(), log);
    ProbeComponent b(2, "b", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a, {b.info().id()}).has_value());
    KRITVA_CHECK(host.add_component(b).has_value());

    std::size_t steps = 0;
    const auto r = host.run([&](LifecycleState state) {
        ++steps;
        const auto o = host.observe();
        KRITVA_CHECK(o.has_value());
        KRITVA_CHECK(o.value().state == state && o.value().state == host.state());
        for (const auto& c : o.value().components) {
            KRITVA_CHECK(c.observation.lifecycle == (state == LifecycleState::READY ? LifecycleState::READY : state));
            KRITVA_CHECK(c.observation.health.state() ==
                         (state == LifecycleState::RUNNING ? HealthState::HEALTHY : HealthState::UNKNOWN));
        }
        KRITVA_CHECK(events.size() == steps);                        // one LIFECYCLE event per completed step
        KRITVA_CHECK(events.snapshot().back().type == EventType::LIFECYCLE);
    });
    KRITVA_CHECK(r.has_value() && steps == 3);
    KRITVA_CHECK(events.size() == 4);                                // + shutdown
    KRITVA_CHECK(events.dropped() == 0);
    std::printf("observation_integration_test: PASS\n");
    return 0;
}
