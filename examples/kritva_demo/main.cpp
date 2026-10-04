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
// Requirements: RR-APP-001..004
// API         : main
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <cstdio>

#include <kritva/runtime/runtime_host.hpp>

int main(int argc, char** argv) {
    using kritva::runtime::to_string;

    kritva::runtime::RuntimeHost host;

    // Optional: kritva_demo <config-file>. Without one, the defaults apply.
    if (argc > 1) {
        const auto config = kritva::runtime::load_configuration_file(argv[1]);
        if (!config) {
            std::printf("[kritva_demo] CONFIG ERROR: %s\n", config.error().message.c_str());
            return 1;
        }
        if (const auto r = host.configure(config.value()); !r) {
            std::printf("[kritva_demo] CONFIG ERROR: %s\n", r.error().message.c_str());
            return 1;
        }
        std::printf("[kritva_demo] config: %s\n",
                    kritva::runtime::describe_configuration(host.settings(), config.value().size()).c_str());
    }

    std::printf("[kritva_demo] state=%s\n", to_string(host.state()).data());

    const auto result = host.run([](kritva::core::LifecycleState state) {
        std::printf("[kritva_demo] state=%s\n", to_string(state).data());
    });

    if (!result) {
        std::printf("[kritva_demo] FAILED: %s\n", result.error().message.c_str());
        return 1;
    }
    std::printf("[kritva_demo] shutdown complete\n");
    return 0;
}
