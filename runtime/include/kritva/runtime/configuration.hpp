//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : configuration.hpp
// Description : Runtime configuration: key=value loader, typed validation, runtime settings.
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

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <kritva/core/core.hpp>

namespace kritva::runtime {

/// Parses `key=value` text into a Core Configuration (no second representation).
///
/// Format: one `key=value` per line; blank lines and lines starting with `#`
/// are ignored; whitespace around key and value is trimmed. Keys use
/// [a-z0-9_.] and are namespaced by component, e.g. `runtime.tick_ms`,
/// `sensor.enabled`. Values become bool (`true`/`false`), int64 (decimal) or
/// string. Anything else, a repeated key, or an invalid key fails with
/// CONFIGURATION_ERROR naming the line number; error text never echoes a value.
[[nodiscard]] core::Result<core::Configuration> parse_configuration(std::string_view text);

/// Reads and parses a configuration file. An unreadable file fails with
/// CONFIGURATION_ERROR (the path is not part of any value).
[[nodiscard]] core::Result<core::Configuration> load_configuration_file(const std::string& path);

/// Typed readers for component code. A missing key yields `default_value`;
/// a present key of the wrong type or out of range fails with
/// CONFIGURATION_ERROR naming the key (never the value).
[[nodiscard]] core::Result<bool> get_bool(const core::Configuration& cfg, const std::string& key, bool default_value);
[[nodiscard]] core::Result<std::int64_t> get_int(const core::Configuration& cfg, const std::string& key,
                                                 std::int64_t default_value, std::int64_t min, std::int64_t max);
[[nodiscard]] core::Result<std::string> get_string(const core::Configuration& cfg, const std::string& key,
                                                   const std::string& default_value);

/// Runtime-level settings (RR-CFG-001).
struct RuntimeSettings {
    std::string name;                 ///< `runtime.name`, required, non-empty.
    std::int64_t tick_ms{100};        ///< `runtime.tick_ms`, optional, 1..60000, default 100.
};

/// Validates and extracts the runtime-level settings (RR-CFG-003).
[[nodiscard]] core::Result<RuntimeSettings> read_runtime_settings(const core::Configuration& cfg);

/// Diagnostic identification of the active configuration (RR-CFG-006):
/// `runtime.name=<name> runtime.tick_ms=<n> parameters=<count>`. Only the known
/// runtime settings and the parameter count are shown; arbitrary parameter
/// values (which could hold secrets) are never printed (RR-SEC-001).
[[nodiscard]] std::string describe_configuration(const RuntimeSettings& settings, std::size_t parameter_count);

} // namespace kritva::runtime
