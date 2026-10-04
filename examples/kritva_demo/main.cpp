//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : main.cpp
// Description : Kritva demo application entry point (KOS-I2): runs the runtime lifecycle.
//
// Component   : KritvaOS Runtime
// Module      : Application
// Layer       : Application Runtime
//
// Requirements: RR-APP-001..006
// API         : main
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <iostream>

#include <kritva/runtime/configuration.hpp>

#include "demo_app.hpp"

// kritva_demo [config-file]
// Exit code: 0 clean run, 1 error, 3 component failure observed and handled by controlled shutdown.
int main(int argc, char** argv) {
    const auto config = argc > 1 ? kritva::runtime::load_configuration_file(argv[1])
                                 : kritva::runtime::parse_configuration("runtime.name=kritva_demo\n");
    if (!config) {
        std::cout << "[kritva_demo] CONFIG ERROR: " << config.error().message << "\n";
        return kritva::demo::exit_code(kritva::demo::DemoOutcome::ERROR);
    }
    kritva::demo::DemoApplication app(std::cout);
    return kritva::demo::exit_code(app.run(config.value()));
}
