//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : link_config.hpp
// Description : Link timing configuration keys: allow-list and validated loading into LinkTiming.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Transport
// Layer       : Hardware Abstraction
//
// Requirements: TR-003
// API         : kritva::hardware::transport::link_config_keys / link_timing_from
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <string>
#include <vector>

#include <kritva/core/core.hpp>
#include <kritva/hardware/transport/protocol.hpp>

namespace kritva::hardware::transport {

/// The four `link.*` keys, for the configuration loader's allow-list so that typos are rejected.
[[nodiscard]] std::vector<std::string> link_config_keys();

/// Reads the `link.*` keys of a configuration over the defaults. Nothing is returned unless the
/// whole result is valid (integers only, each inside its range, and the cross-constraints of the
/// specification hold), so a caller commits all four values or none. CONFIGURATION_ERROR names
/// the offending key. Other keys are ignored.
[[nodiscard]] core::Result<LinkTiming> link_timing_from(const core::Configuration& configuration);

} // namespace kritva::hardware::transport
