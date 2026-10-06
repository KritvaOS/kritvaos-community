//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : topology.hpp
// Description : The discovered Edge topology as a set of identity tuples, the typed-mapping check and the
//               set-based equivalence of protocol section 13.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: ER-002; RR-001
// API         : kritva::hardware::remote::Topology / topology_of / topology_equivalent / check_supported
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <kritva/hardware/remote/node.hpp>
#include <kritva/hardware/transport/codec.hpp>

namespace kritva::hardware::remote {

/// One endpoint of a node, with everything that identifies it (protocol section 13).
struct TopologyEntry {
    std::uint64_t node{0};
    std::uint64_t device_id{0};
    std::string device_name;
    std::uint64_t endpoint_id{0};
    std::string endpoint_name;
    transport::WireDirection direction{transport::WireDirection::SENSOR};
    std::vector<std::uint64_t> capability_ids;           // sorted
    friend bool operator==(const TopologyEntry&, const TopologyEntry&) = default;
};

using Topology = std::vector<TopologyEntry>;

/// The identity tuples of a discovery response, one per endpoint, capability ids sorted. Order of the result
/// follows the response; equivalence ignores it.
[[nodiscard]] Topology topology_of(NodeId node, const transport::DiscoveryResponsePayload& discovery);

/// Set equality of the identity tuples: the same (node, device id and name, endpoint id and name, direction,
/// capability ids) tuples, in any order. Any difference (an added, removed or changed endpoint, a renamed
/// device, a changed direction or capability) makes the topologies not equivalent.
[[nodiscard]] bool topology_equivalent(const Topology& a, const Topology& b);

/// The typed mapping of protocol section 13: a sensor endpoint must advertise exactly one of 0x1001, 0x1002
/// or 0x1003 and an actuator exactly 0x2001. Anything else is UNSUPPORTED (no silent generic endpoint).
[[nodiscard]] core::Result<void> check_supported(const transport::DiscoveryResponsePayload& discovery);

} // namespace kritva::hardware::remote
