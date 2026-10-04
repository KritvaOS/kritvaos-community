//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : configuration_test.cpp
// Description : Unit tests of runtime configuration: parsing, typed readers, settings, host configure.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-CFG-001..006; RR-SEC-001; RR-SEC-002; RR-TST-004
// API         : RUNTIME-CONFIG-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <filesystem>
#include <fstream>
#include <string>
#include <variant>
#include <vector>

#include "../check.hpp"
#include "../probe_component.hpp"

using namespace kritva;
using namespace kritva::runtime;
using kritva::core::ErrorCode;
using kritva::core::LifecycleState;
using Log = std::vector<std::string>;

static bool fails_with_config_error(const core::Result<core::Configuration>& r) {
    return !r.has_value() && r.error().code == ErrorCode::CONFIGURATION_ERROR;
}

static void test_parse_valid() {                                     // RR-CFG-001/002
    const auto r = parse_configuration(
        "# comment\n\n  runtime.name = demo  \r\nruntime.tick_ms=50\nsensor.enabled=true\nsensor.label=hello world\nmonitor.limit=-3\n");
    KRITVA_CHECK(r.has_value());
    const auto& c = r.value();
    KRITVA_CHECK(c.size() == 5);
    KRITVA_CHECK(std::get<std::string>(c.get("runtime.name")->value) == "demo");
    KRITVA_CHECK(std::get<std::int64_t>(c.get("runtime.tick_ms")->value) == 50);
    KRITVA_CHECK(std::get<bool>(c.get("sensor.enabled")->value) == true);
    KRITVA_CHECK(std::get<std::string>(c.get("sensor.label")->value) == "hello world");
    KRITVA_CHECK(std::get<std::int64_t>(c.get("monitor.limit")->value) == -3);
}

static void test_parse_empty_and_no_trailing_newline() {
    KRITVA_CHECK(parse_configuration("").value().size() == 0);
    KRITVA_CHECK(parse_configuration("# only a comment").value().size() == 0);
    KRITVA_CHECK(parse_configuration("a.b=1").value().size() == 1);
}

static void test_parse_errors_are_deterministic() {                  // RR-CFG-004/005
    auto r = parse_configuration("runtime.name=x\nthis line has no equals\n");
    KRITVA_CHECK(fails_with_config_error(r));
    KRITVA_CHECK(r.error().message == "line 2: expected key=value");
    KRITVA_CHECK(parse_configuration("runtime.name=x\nruntime.name=y\n").error().message
                 == "line 2: duplicate key 'runtime.name'");
    KRITVA_CHECK(fails_with_config_error(parse_configuration("=value\n")));          // empty key
    KRITVA_CHECK(fails_with_config_error(parse_configuration("Bad-Key=1\n")));       // invalid chars
    KRITVA_CHECK(parse_configuration("Bad-Key=1\n").error().message == "line 1: invalid key");
    KRITVA_CHECK(parse_configuration("x=\n").has_value());                           // empty value is a string
}

static void test_errors_never_echo_values() {                        // RR-SEC-001
    const auto r = parse_configuration("password.token.secret\nruntime.name=s3cr3t-value\nruntime.name=s3cr3t-value\n");
    KRITVA_CHECK(!r.has_value());
    KRITVA_CHECK(r.error().message.find("s3cr3t") == std::string::npos);
    const auto cfg = parse_configuration("runtime.name=ok\nruntime.tick_ms=s3cr3t\n").value();
    const auto s = read_runtime_settings(cfg);
    KRITVA_CHECK(!s.has_value() && s.error().message.find("s3cr3t") == std::string::npos);
}

static void test_load_file_missing() {
    KRITVA_CHECK(fails_with_config_error(load_configuration_file("/nonexistent/kritva.conf")));
}

static void test_typed_readers() {
    const auto cfg = parse_configuration("a=true\nb=7\nc=text\n").value();
    KRITVA_CHECK(get_bool(cfg, "a", false).value() == true);
    KRITVA_CHECK(get_bool(cfg, "missing", true).value() == true);                    // default
    KRITVA_CHECK(!get_bool(cfg, "b", false).has_value());                            // wrong type
    KRITVA_CHECK(get_int(cfg, "b", 0, 1, 10).value() == 7);
    KRITVA_CHECK(get_int(cfg, "missing", 4, 1, 10).value() == 4);
    KRITVA_CHECK(!get_int(cfg, "b", 0, 8, 10).has_value());                          // out of range
    KRITVA_CHECK(!get_int(cfg, "c", 0, 1, 10).has_value());                          // wrong type
    KRITVA_CHECK(get_string(cfg, "c", "").value() == "text");
    KRITVA_CHECK(get_string(cfg, "missing", "dflt").value() == "dflt");
    KRITVA_CHECK(!get_string(cfg, "b", "").has_value());
}

static void test_runtime_settings() {                                // RR-CFG-001/003
    auto s = read_runtime_settings(parse_configuration("runtime.name=demo\nruntime.tick_ms=20\n").value());
    KRITVA_CHECK(s.has_value() && s.value().name == "demo" && s.value().tick_ms == 20);
    s = read_runtime_settings(parse_configuration("runtime.name=demo\n").value());
    KRITVA_CHECK(s.has_value() && s.value().tick_ms == 100);                         // explicit default
    KRITVA_CHECK(!read_runtime_settings(parse_configuration("runtime.tick_ms=20\n").value()).has_value());   // name required
    KRITVA_CHECK(!read_runtime_settings(parse_configuration("runtime.name=\n").value()).has_value());         // empty name
    KRITVA_CHECK(!read_runtime_settings(parse_configuration("runtime.name=1\n").value()).has_value());        // not a string
    KRITVA_CHECK(!read_runtime_settings(parse_configuration("runtime.name=x\nruntime.tick_ms=0\n").value()).has_value());
    KRITVA_CHECK(!read_runtime_settings(parse_configuration("runtime.name=x\nruntime.tick_ms=60001\n").value()).has_value());
    KRITVA_CHECK(!read_runtime_settings(parse_configuration("runtime.name=x\nruntime.tick_ms=fast\n").value()).has_value());
}

static void test_describe() {                                        // RR-CFG-006
    RuntimeSettings s{"demo", 25};
    KRITVA_CHECK(describe_configuration(s, 3) == "runtime.name=demo runtime.tick_ms=25 parameters=3");
}

static void test_unknown_keys_rejected() {                           // RR-CFG-004
    auto r = parse_configuration("runtime.name=demo\nruntime.nmae=oops\n");   // typo in the runtime namespace
    KRITVA_CHECK(fails_with_config_error(r));
    KRITVA_CHECK(r.error().message == "line 2: unknown key 'runtime.nmae'");
    KRITVA_CHECK(parse_configuration("runtime.name=demo\nruntime.tick_ms=5\n").has_value());   // known keys pass
    KRITVA_CHECK(parse_configuration("other.key=1\n").has_value());                              // no list: not checked

    // With an allow-list every key outside the runtime namespace must be listed.
    const std::vector<std::string> allowed{"sensor.enabled", "sensor.failure_after_ticks"};
    KRITVA_CHECK(parse_configuration("runtime.name=x\nsensor.enabled=true\n", allowed).has_value());
    r = parse_configuration("runtime.name=x\nsensor.failure_after_tick=3\n", allowed);          // typo
    KRITVA_CHECK(fails_with_config_error(r) && r.error().message == "line 2: unknown key 'sensor.failure_after_tick'");
    KRITVA_CHECK(fails_with_config_error(parse_configuration("runtime.bogus=1\n", allowed)));
}

static std::string comment_text(std::size_t bytes) {                 // `bytes` bytes of valid configuration text
    std::string text = "runtime.name=x\n";
    while (text.size() + 2 <= bytes) text += "#\n";
    while (text.size() < bytes) text += " ";
    return text;
}

static void test_size_limit() {                                      // RR-PERF-002, RR-SEC-002
    const auto at_limit = comment_text(kMaxConfigurationBytes);
    KRITVA_CHECK(at_limit.size() == kMaxConfigurationBytes);
    KRITVA_CHECK(parse_configuration(at_limit).has_value());                         // configuration_at_limit_is_accepted
    const auto over = comment_text(kMaxConfigurationBytes + 1);
    KRITVA_CHECK(over.size() == kMaxConfigurationBytes + 1);
    const auto r = parse_configuration(over);                                        // configuration_over_limit_is_rejected
    KRITVA_CHECK(fails_with_config_error(r) && r.error().message == "configuration exceeds 65536 bytes");

    const auto dir = std::filesystem::temp_directory_path();
    const auto good = (dir / "kritva_i2_cfg_at_limit.conf").string();
    const auto bad = (dir / "kritva_i2_cfg_over_limit.conf").string();
    std::ofstream(good, std::ios::binary) << at_limit;
    std::ofstream(bad, std::ios::binary) << over;
    KRITVA_CHECK(load_configuration_file(good).has_value());
    const auto f = load_configuration_file(bad);
    KRITVA_CHECK(fails_with_config_error(f) && f.error().message == "configuration exceeds 65536 bytes");
    std::filesystem::remove(good);
    std::filesystem::remove(bad);
}

static core::Configuration with_name(const std::string& name) {
    core::Configuration c;
    KRITVA_CHECK(c.set(core::Parameter{"runtime.name", name, {}}).has_value());
    return c;
}

static void test_runtime_name_validation() {                         // diagnostics integrity
    KRITVA_CHECK(read_runtime_settings(with_name("kritva_demo-1.0")).has_value());
    KRITVA_CHECK(read_runtime_settings(with_name(std::string(64, 'a'))).has_value());
    KRITVA_CHECK(!read_runtime_settings(with_name(std::string(65, 'a'))).has_value());            // too long
    for (const std::string& bad : {std::string("a\nSECOND=1"), std::string("a\rb"), std::string("a\tb"),
                                  std::string("a b"), std::string("a\x01" "b"), std::string("a") + '\0' + "b",
                                  std::string("a\x7f" "b"), std::string("na\xc3\xafve")}) {
        const auto r = read_runtime_settings(with_name(bad));
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::CONFIGURATION_ERROR);
    }
}

static void test_host_configure() {
    RuntimeHost host;
    Log log;
    runtime::test::ProbeComponent a(1, "a", host.runtime(), log);
    runtime::test::ProbeComponent b(2, "b", host.runtime(), log);
    a.log_configure(true);
    b.log_configure(true);
    KRITVA_CHECK(host.add_component(a, {b.info().id()}).has_value());
    KRITVA_CHECK(host.add_component(b).has_value());

    // Invalid runtime settings: nothing configured, settings unchanged.
    const auto bad = parse_configuration("runtime.tick_ms=20\n").value();
    auto r = host.configure(bad);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::CONFIGURATION_ERROR);
    KRITVA_CHECK(log.empty() && host.settings().name.empty() && host.settings().tick_ms == 100);
    KRITVA_CHECK(host.state() == LifecycleState::UNKNOWN);

    // Valid: components configured in dependency order, settings active, state unchanged.
    const auto good = parse_configuration("runtime.name=demo\nruntime.tick_ms=20\n").value();
    KRITVA_CHECK(host.configure(good).has_value());
    const Log expected{"b.configure:UNKNOWN", "a.configure:UNKNOWN"};
    KRITVA_CHECK(log == expected);
    KRITVA_CHECK(host.settings().name == "demo" && host.settings().tick_ms == 20);
    KRITVA_CHECK(host.state() == LifecycleState::UNKNOWN);

    // Configuration is rejected once the runtime is live (Core rule), settings keep their value.
    KRITVA_CHECK(host.initialize().has_value());
    r = host.configure(parse_configuration("runtime.name=other\n").value());
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_STATE);
    KRITVA_CHECK(host.settings().name == "demo");
}

int main() {
    test_parse_valid();
    test_parse_empty_and_no_trailing_newline();
    test_parse_errors_are_deterministic();
    test_errors_never_echo_values();
    test_load_file_missing();
    test_typed_readers();
    test_runtime_settings();
    test_describe();
    test_unknown_keys_rejected();
    test_size_limit();
    test_runtime_name_validation();
    test_host_configure();
    std::printf("configuration_test: PASS\n");
    return 0;
}
