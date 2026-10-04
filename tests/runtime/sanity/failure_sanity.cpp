//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : failure_sanity.cpp
// Description : Sanity executable: failure injection flow printed step by step.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-FLT-001..006
// API         : RUNTIME-FAILURE-SANITY
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
    ProbeComponent sensor(1, "sensor", host.runtime(), log);
    ProbeComponent monitor(2, "monitor", host.runtime(), log);
    sensor.set_event_sink(&events);
    KRITVA_CHECK(host.add_component(sensor).has_value());
    KRITVA_CHECK(host.add_component(monitor, {sensor.info().id()}).has_value());

    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.initialize().has_value() && host.start().has_value());
    std::printf("[failure] RUNNING\n");
    sensor.inject_failure();
    std::printf("[failure] injected\n%s", runtime::describe(host.observe().value()).c_str());
    const auto report = host.failure_report().value();
    std::printf("[failure] failed=%zu affected=%zu\n", report.failed.size(), report.affected.size());
    for (const auto& e : events.snapshot()) {
        if (e.type == core::EventType::ERROR) std::printf("[failure] event ERROR source=%llu\n", static_cast<unsigned long long>(e.source_id.value()));
    }
    KRITVA_CHECK(host.controlled_shutdown().has_value());
    std::printf("[failure] runtime state=%s\n", std::string(runtime::to_string(host.state())).c_str());
    return 0;
}
