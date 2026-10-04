//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : composition_sanity.cpp
// Description : Sanity executable: composes three components, runs the lifecycle and prints the order.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-CMP-004; RR-CMP-005; RR-TST-003
// API         : RUNTIME-COMPOSITION-SANITY
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
    std::vector<std::string> log;
    ProbeComponent c(3, "third", host.runtime(), log);
    ProbeComponent b(2, "second", host.runtime(), log);
    ProbeComponent a(1, "first", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a, {b.info().id()}).has_value());
    KRITVA_CHECK(host.add_component(b, {c.info().id()}).has_value());
    KRITVA_CHECK(host.add_component(c).has_value());
    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.run().has_value());
    for (const auto& entry : log) std::printf("[composition] %s\n", entry.c_str());
    return 0;
}
