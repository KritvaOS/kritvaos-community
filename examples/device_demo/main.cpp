//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : main.cpp
// Description : KOS-I3 reference device demo entry point.
//
// Component   : KritvaOS Demo
// Module      : Device Demo
// Layer       : Application
//
// Requirements: DER-804
// API         : main
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <iostream>

#include <kritva/runtime/configuration.hpp>

#include "device_demo_app.hpp"

// kritva_device_demo [config-file]
// Exit code: 0 the scenario completed as expected, 1 an error or an unexpected result.
int main(int argc, char** argv) {
    using namespace kritva::demo;
    const auto keys = device_demo_config_keys();
    const auto config = argc > 1 ? kritva::runtime::load_configuration_file(argv[1], keys)
                                 : kritva::runtime::parse_configuration("runtime.name=kritva_device_demo\n", keys);
    if (!config) {
        std::cout << "[device_demo] CONFIG ERROR: " << config.error().message << "\n";
        return exit_code(DeviceDemoOutcome::ERROR);
    }
    DeviceDemoApplication app(std::cout);
    return exit_code(app.run(config.value()));
}
