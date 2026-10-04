//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : configuration.cpp
// Description : Runtime configuration implementation.
//
// Component   : KritvaOS Runtime
// Module      : Configuration
// Layer       : Application Runtime
//
// Requirements: RR-CFG-001..006; RR-SEC-001; RR-SEC-002
// API         : kritva::runtime::parse_configuration
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <kritva/runtime/configuration.hpp>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <variant>
#include <vector>

namespace kritva::runtime {

using core::Configuration;
using core::Error;
using core::ErrorCode;
using core::ErrorSeverity;
using core::Parameter;
using core::Result;

namespace {

Error config_error(std::string message) {
    return Error{ErrorCode::CONFIGURATION_ERROR, ErrorSeverity::ERROR, {}, {}, std::move(message)};
}

std::string_view trim(std::string_view s) {
    constexpr std::string_view ws = " \t\r";
    const auto first = s.find_first_not_of(ws);
    if (first == std::string_view::npos) return {};
    return s.substr(first, s.find_last_not_of(ws) - first + 1);
}

bool valid_key(std::string_view key) {
    return !key.empty() && std::all_of(key.begin(), key.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
    });
}

core::ParameterValue parse_value(std::string_view v) {
    if (v == "true") return true;
    if (v == "false") return false;
    std::int64_t number = 0;
    const auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), number);
    if (!v.empty() && ec == std::errc{} && end == v.data() + v.size()) return number;
    return std::string(v);
}

} // namespace

Result<Configuration> parse_configuration(std::string_view text) {
    Configuration cfg;
    std::size_t line_no = 0;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const auto eol = text.find('\n', pos);
        const auto raw = text.substr(pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
        pos = eol == std::string_view::npos ? text.size() + 1 : eol + 1;
        ++line_no;

        const auto line = trim(raw);
        if (line.empty() || line.front() == '#') continue;
        const std::string where = "line " + std::to_string(line_no) + ": ";

        const auto eq = line.find('=');
        if (eq == std::string_view::npos) return Result<Configuration>::failure(config_error(where + "expected key=value"));
        const auto key = trim(line.substr(0, eq));
        if (!valid_key(key)) return Result<Configuration>::failure(config_error(where + "invalid key"));
        const std::string name(key);
        if (cfg.contains(name)) return Result<Configuration>::failure(config_error(where + "duplicate key '" + name + "'"));

        if (auto r = cfg.set(Parameter{name, parse_value(trim(line.substr(eq + 1))), {}}); !r) {
            return Result<Configuration>::failure(r.error());
        }
    }
    if (auto r = cfg.validate(); !r) return Result<Configuration>::failure(r.error());
    return Result<Configuration>::success(std::move(cfg));
}

Result<Configuration> load_configuration_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) return Result<Configuration>::failure(config_error("cannot read configuration file"));
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return parse_configuration(buffer.str());
}

Result<bool> get_bool(const Configuration& cfg, const std::string& key, bool default_value) {
    const Parameter* p = cfg.get(key);
    if (!p) return Result<bool>::success(default_value);
    if (const bool* v = std::get_if<bool>(&p->value)) return Result<bool>::success(*v);
    return Result<bool>::failure(config_error("'" + key + "' must be true or false"));
}

Result<std::int64_t> get_int(const Configuration& cfg, const std::string& key,
                             std::int64_t default_value, std::int64_t min, std::int64_t max) {
    const Parameter* p = cfg.get(key);
    if (!p) return Result<std::int64_t>::success(default_value);
    const auto* v = std::get_if<std::int64_t>(&p->value);
    if (!v) return Result<std::int64_t>::failure(config_error("'" + key + "' must be an integer"));
    if (*v < min || *v > max) {
        return Result<std::int64_t>::failure(config_error(
            "'" + key + "' out of range [" + std::to_string(min) + ", " + std::to_string(max) + "]"));
    }
    return Result<std::int64_t>::success(*v);
}

Result<std::string> get_string(const Configuration& cfg, const std::string& key, const std::string& default_value) {
    const Parameter* p = cfg.get(key);
    if (!p) return Result<std::string>::success(default_value);
    if (const auto* v = std::get_if<std::string>(&p->value)) return Result<std::string>::success(*v);
    return Result<std::string>::failure(config_error("'" + key + "' must be a string"));
}

Result<RuntimeSettings> read_runtime_settings(const Configuration& cfg) {
    if (auto r = cfg.validate(); !r) return Result<RuntimeSettings>::failure(r.error());
    if (!cfg.contains("runtime.name")) return Result<RuntimeSettings>::failure(config_error("'runtime.name' is required"));
    auto name = get_string(cfg, "runtime.name", {});
    if (!name) return Result<RuntimeSettings>::failure(name.error());
    if (name.value().empty()) return Result<RuntimeSettings>::failure(config_error("'runtime.name' must not be empty"));
    auto tick = get_int(cfg, "runtime.tick_ms", 100, 1, 60000);
    if (!tick) return Result<RuntimeSettings>::failure(tick.error());
    return Result<RuntimeSettings>::success(RuntimeSettings{std::move(name).value(), tick.value()});
}

std::string describe_configuration(const RuntimeSettings& settings, std::size_t parameter_count) {
    return "runtime.name=" + settings.name + " runtime.tick_ms=" + std::to_string(settings.tick_ms) +
           " parameters=" + std::to_string(parameter_count);
}

} // namespace kritva::runtime
