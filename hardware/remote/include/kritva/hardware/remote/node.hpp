//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : node.hpp
// Description : Node, session and endpoint-address identity for the Nexus-Edge layer.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote Identity
// Layer       : Hardware Abstraction
//
// Requirements: NDR-001; NDR-002; NDR-003
// API         : kritva::hardware::remote::NodeId / SessionId / EndpointAddress
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <compare>
#include <cstdint>

#include <kritva/hardware/identity.hpp>

namespace kritva::hardware::remote {

struct NodeTag {};
struct SessionTag {};

/// Strong identifier of a Nexus or Edge node (valid iff non-zero). Distinct from DeviceId and
/// EndpointId, which may collide between different nodes (NDR-002).
using NodeId = TaggedId<NodeTag>;

/// Identifier of one session between a Nexus and an Edge (valid iff non-zero). The Edge allocates
/// 1, 2, 3 ... per EdgeHost lifetime; every fresh initialization establishes a new one (NDR-003).
using SessionId = TaggedId<SessionTag>;

/// The address of an endpoint across nodes: (NodeId, DeviceId, EndpointId) (NDR-001). Names are
/// human-readable metadata and are never part of an address. Valid iff all three are valid.
struct EndpointAddress {
    NodeId node;
    DeviceId device;
    EndpointId endpoint;

    [[nodiscard]] constexpr bool valid() const noexcept { return node.valid() && device.valid() && endpoint.valid(); }
    friend constexpr bool operator==(const EndpointAddress&, const EndpointAddress&) noexcept = default;
    friend constexpr auto operator<=>(const EndpointAddress&, const EndpointAddress&) noexcept = default;
};

} // namespace kritva::hardware::remote
