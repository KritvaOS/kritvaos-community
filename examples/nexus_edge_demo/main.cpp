//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : main.cpp
// Description : Entry point of the KOS-I4 Nexus-Edge reference demo.
//
// Component   : KritvaOS Examples
// Module      : Nexus-Edge Demo
// Layer       : Application
//
// Requirements: VR-003
// API         : kritva_nexus_edge_demo
//
// Author      : KritvaOS
// Created     : 07-10-2026
//==============================================================================

#include <iostream>

#include <kritva/runtime/configuration.hpp>

#include "nexus_edge_demo_app.hpp"

// kritva_nexus_edge_demo [config-file]
// Exit code: 0 the scenario completed as expected, 1 an error or an unexpected result.
int main(int argc, char** argv) {
    using namespace kritva::demo;
    const auto keys = nexus_edge_demo_config_keys();
    const auto config = argc > 1 ? kritva::runtime::load_configuration_file(argv[1], keys)
                                 : kritva::runtime::parse_configuration("runtime.name=kritva_nexus_edge_demo\n", keys);
    if (!config) {
        std::cout << "[nexus_edge_demo] CONFIG ERROR: " << config.error().message << "\n";
        return exit_code(NexusEdgeDemoOutcome::ERROR);
    }
    NexusEdgeDemoApplication app(std::cout);
    return exit_code(app.run(config.value()));
}
