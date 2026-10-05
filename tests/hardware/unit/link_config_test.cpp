//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : link_config_test.cpp
// Description : Unit tests of link timing configuration loading (the link.* keys).
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: TR-003
// API         : LINK-CONFIG-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <cstdint>
#include <string>

#include "../../runtime/check.hpp"

#include <kritva/hardware/transport/link_config.hpp>

using namespace kritva::hardware::transport;
using kritva::core::Configuration;
using kritva::core::ErrorCode;
using kritva::core::Parameter;

static Configuration cfg(std::initializer_list<std::pair<const char*, std::int64_t>> values) {
    Configuration c;
    for (const auto& [k, v] : values) KRITVA_CHECK(c.set(Parameter{k, v, {}}).has_value());
    return c;
}

static void test_defaults_and_keys() {
    const auto keys = link_config_keys();
    KRITVA_CHECK(keys.size() == 4 && keys[0] == "link.heartbeat_period_ms" && keys[1] == "link.heartbeat_timeout_ms" &&
                 keys[2] == "link.request_timeout_ms" && keys[3] == "link.pump_quantum_ms");
    const auto t = link_timing_from(Configuration{});
    KRITVA_CHECK(t.has_value());
    KRITVA_CHECK(t.value().heartbeat_period_ms == 100 && t.value().heartbeat_timeout_ms == 300 &&
                 t.value().request_timeout_ms == 100 && t.value().pump_quantum_ms == 1);   // the agreed defaults
    KRITVA_CHECK(link_timing_from(cfg({{"other.key", 5}})).has_value());                  // other keys are ignored
}

static void test_override_and_partial() {
    auto t = link_timing_from(cfg({{"link.heartbeat_period_ms", 50}, {"link.heartbeat_timeout_ms", 200},
                                   {"link.request_timeout_ms", 20}, {"link.pump_quantum_ms", 5}}));
    KRITVA_CHECK(t.has_value() && t.value().heartbeat_period_ms == 50 && t.value().heartbeat_timeout_ms == 200 &&
                 t.value().request_timeout_ms == 20 && t.value().pump_quantum_ms == 5);
    t = link_timing_from(cfg({{"link.request_timeout_ms", 250}}));
    KRITVA_CHECK(t.has_value() && t.value().request_timeout_ms == 250 && t.value().heartbeat_period_ms == 100);   // others keep defaults
}

static void expect_rejected(const Configuration& c, const char* what) {
    const auto t = link_timing_from(c);
    if (t.has_value() || t.error().code != ErrorCode::CONFIGURATION_ERROR) {
        std::fprintf(stderr, "not rejected: %s\n", what);
        std::exit(1);
    }
}

static void test_ranges_and_cross_constraints() {
    expect_rejected(cfg({{"link.heartbeat_period_ms", 9}}), "period below 10");
    expect_rejected(cfg({{"link.heartbeat_period_ms", 60001}, {"link.heartbeat_timeout_ms", 600000}}), "period above 60000");
    expect_rejected(cfg({{"link.heartbeat_timeout_ms", 19}, {"link.heartbeat_period_ms", 10}}), "timeout below 20");
    expect_rejected(cfg({{"link.heartbeat_timeout_ms", 600001}}), "timeout above 600000");
    expect_rejected(cfg({{"link.heartbeat_period_ms", 200}}), "timeout 300 < 2 x period 200");
    expect_rejected(cfg({{"link.request_timeout_ms", 0}}), "request timeout 0");
    expect_rejected(cfg({{"link.request_timeout_ms", 60001}}), "request timeout above 60000");
    expect_rejected(cfg({{"link.pump_quantum_ms", 0}}), "quantum 0");
    expect_rejected(cfg({{"link.pump_quantum_ms", 1001}, {"link.request_timeout_ms", 2000}}), "quantum above 1000");
    expect_rejected(cfg({{"link.pump_quantum_ms", 101}}), "quantum above the request timeout");
    // Boundaries that are valid.
    KRITVA_CHECK(link_timing_from(cfg({{"link.heartbeat_period_ms", 10}, {"link.heartbeat_timeout_ms", 20}})).has_value());
    KRITVA_CHECK(link_timing_from(cfg({{"link.heartbeat_period_ms", 60000}, {"link.heartbeat_timeout_ms", 120000}})).has_value());
    KRITVA_CHECK(link_timing_from(cfg({{"link.heartbeat_period_ms", 300}, {"link.heartbeat_timeout_ms", 600}})).has_value());
    KRITVA_CHECK(link_timing_from(cfg({{"link.heartbeat_period_ms", 100}, {"link.heartbeat_timeout_ms", 200}})).has_value());   // exactly 2 x
    KRITVA_CHECK(link_timing_from(cfg({{"link.pump_quantum_ms", 100}})).has_value());                                          // quantum == request timeout
}

static void test_wrong_types_and_values() {
    Configuration c;
    KRITVA_CHECK(c.set(Parameter{"link.heartbeat_period_ms", std::string("fast"), {}}).has_value());
    expect_rejected(c, "string value");
    Configuration d;
    KRITVA_CHECK(d.set(Parameter{"link.request_timeout_ms", 1.5, {}}).has_value());
    expect_rejected(d, "floating value");
    Configuration e;
    KRITVA_CHECK(e.set(Parameter{"link.pump_quantum_ms", true, {}}).has_value());
    expect_rejected(e, "boolean value");
    expect_rejected(cfg({{"link.request_timeout_ms", -1}}), "negative");
    expect_rejected(cfg({{"link.request_timeout_ms", static_cast<std::int64_t>(UINT32_MAX) + 1}}), "above 32 bits");
    expect_rejected(cfg({{"link.heartbeat_period_ms", INT64_MAX}}), "huge");
    // A value that would be a valid timing if it were silently truncated to 32 bits is rejected, not wrapped.
    expect_rejected(cfg({{"link.request_timeout_ms", (std::int64_t{1} << 32) + 50}}), "2^32 + 50 would wrap to 50");
    expect_rejected(cfg({{"link.pump_quantum_ms", (std::int64_t{1} << 32) + 1}}), "2^32 + 1 would wrap to 1");
    // The offending key is named for a negative value too.
    const auto neg = link_timing_from(cfg({{"link.request_timeout_ms", -1}}));
    KRITVA_CHECK(!neg.has_value() && neg.error().message.find("link.request_timeout_ms") != std::string::npos);
}

static void test_all_or_nothing_and_message() {
    // One bad key among good ones: nothing is returned, and the message names the problem.
    const auto t = link_timing_from(cfg({{"link.heartbeat_period_ms", 50}, {"link.request_timeout_ms", 0}}));
    KRITVA_CHECK(!t.has_value() && t.error().message.find("link timing") != std::string::npos);
    Configuration c;
    KRITVA_CHECK(c.set(Parameter{"link.heartbeat_timeout_ms", std::string("x"), {}}).has_value());
    const auto u = link_timing_from(c);
    KRITVA_CHECK(!u.has_value() && u.error().message.find("link.heartbeat_timeout_ms") != std::string::npos);   // names the key
}

int main() {
    test_defaults_and_keys();
    test_override_and_partial();
    test_ranges_and_cross_constraints();
    test_wrong_types_and_values();
    test_all_or_nothing_and_message();
    std::printf("link_config_test: PASS\n");
    return 0;
}
