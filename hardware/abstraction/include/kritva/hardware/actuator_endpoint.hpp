//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : actuator_endpoint.hpp
// Description : Typed actuator endpoint contract: write one command.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Typed Endpoints
// Layer       : Hardware Abstraction
//
// Requirements: DER-502; DER-504; DER-505; DER-507
// API         : kritva::hardware::ActuatorEndpoint
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <cassert>
#include <type_traits>

#include <kritva/hardware/endpoint.hpp>

namespace kritva::hardware {

/// An actuator endpoint consumes one typed `Command` per write(). `Command` is a
/// trivially copyable, default-constructible value type whose units, limits and
/// validity are documented by its own definition.
///
/// write() fails with NOT_READY unless the endpoint is RUNNING (RESOURCE_UNAVAILABLE
/// if it is faulted), without calling the implementation (DER-504). Otherwise
/// do_write()'s result is returned unchanged; an implementation rejects an
/// invalid command with INVALID_ARGUMENT and must not act on it. Every attempt is
/// counted and a failure becomes last_error(). A failing do_write() does not by
/// itself fault the endpoint.
///
/// SAFETY: an actuator write can move a physical system. Implementations validate
/// the command before acting; a rejected command has no effect.
template <class Command>
class ActuatorEndpoint : public Endpoint {
    static_assert(std::is_trivially_copyable_v<Command> && std::is_default_constructible_v<Command>,
                  "an actuator command is a plain value type");

public:
    using command_type = Command;

    core::Result<void> write(const Command& command) {
        auto result = check_operational("write");
        if (result) result = do_write(command);
        count_operation(result.has_value());
        if (!result) note_error(result.error());
        return result;
    }

protected:
    ActuatorEndpoint(EndpointInfo info, core::CapabilitySet capabilities) : Endpoint(std::move(info), std::move(capabilities)) {
        assert(this->info().direction() == EndpointDirection::ACTUATOR);
    }

    /// Validates and applies one command while RUNNING.
    virtual core::Result<void> do_write(const Command& command) = 0;
};

} // namespace kritva::hardware
