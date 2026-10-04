//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : configuration_integration_test.cpp
// Description : Integration: configuration file -> loader -> host -> components -> lifecycle.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-CFG-001..005; RR-TST-004
// API         : RUNTIME-CONFIG-IT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../check.hpp"
#include "../probe_component.hpp"

using namespace kritva;
using kritva::core::ErrorCode;
using kritva::core::LifecycleState;
using kritva::runtime::RuntimeHost;
using kritva::runtime::test::ProbeComponent;

static std::string write_file(const char* name, const std::string& text) {
    const auto path = (std::filesystem::temp_directory_path() / name).string();
    std::ofstream(path) << text;
    return path;
}

int main() {
    // Valid file: loaded, validated, applied to components, then the full lifecycle runs.
    {
        const auto path = write_file("kritva_i2_cfg_valid.conf", "runtime.name=itest\nruntime.tick_ms=10\nsensor.enabled=true\n");
        const auto cfg = runtime::load_configuration_file(path);
        KRITVA_CHECK(cfg.has_value());

        RuntimeHost host;
        std::vector<std::string> log;
        ProbeComponent a(1, "a", host.runtime(), log);
        KRITVA_CHECK(host.add_component(a).has_value());
        KRITVA_CHECK(host.configure(cfg.value()).has_value());
        KRITVA_CHECK(host.settings().name == "itest" && host.settings().tick_ms == 10);
        KRITVA_CHECK(host.run().has_value());
        KRITVA_CHECK(host.state() == LifecycleState::STOPPED);
        KRITVA_CHECK(log.front() == "a.configure:UNKNOWN");           // configured before initialization
        KRITVA_CHECK(runtime::get_bool(cfg.value(), "sensor.enabled", false).value());
        std::filesystem::remove(path);
    }
    // Invalid values: rejected before normal operation; no component is invoked.
    {
        const auto path = write_file("kritva_i2_cfg_invalid.conf", "runtime.name=itest\nruntime.tick_ms=0\n");
        const auto cfg = runtime::load_configuration_file(path);
        KRITVA_CHECK(cfg.has_value());                                // syntactically valid
        RuntimeHost host;
        std::vector<std::string> log;
        ProbeComponent a(1, "a", host.runtime(), log);
        KRITVA_CHECK(host.add_component(a).has_value());
        const auto r = host.configure(cfg.value());
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::CONFIGURATION_ERROR);
        KRITVA_CHECK(log.empty() && host.state() == LifecycleState::UNKNOWN);
        std::filesystem::remove(path);
    }
    std::printf("configuration_integration_test: PASS\n");
    return 0;
}
