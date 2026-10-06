//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : topology.cpp
// Description : Topology identity tuples, typed-mapping check and set equivalence.
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

#include <kritva/hardware/remote/topology.hpp>

#include <algorithm>
#include <tuple>

#include <kritva/hardware/remote/capability_dispatch.hpp>

namespace kritva::hardware::remote {

namespace {

auto key_of(const TopologyEntry& e) {
    return std::tie(e.node, e.device_id, e.device_name, e.endpoint_id, e.endpoint_name, e.direction, e.capability_ids);
}

core::Error unsupported(const char* message) {
    return core::Error{core::ErrorCode::UNSUPPORTED, core::ErrorSeverity::ERROR, {}, {}, message};
}

} // namespace

Topology topology_of(NodeId node, const transport::DiscoveryResponsePayload& discovery) {
    Topology out;
    for (const auto& d : discovery.devices) {
        for (const auto& e : d.endpoints) {
            TopologyEntry t;
            t.node = node.value();
            t.device_id = d.id;
            t.device_name = d.name;
            t.endpoint_id = e.id;
            t.endpoint_name = e.name;
            t.direction = e.direction;
            for (const auto& c : e.capabilities) t.capability_ids.push_back(c.id);
            std::sort(t.capability_ids.begin(), t.capability_ids.end());
            out.push_back(std::move(t));
        }
    }
    return out;
}

bool topology_equivalent(const Topology& a, const Topology& b) {
    if (a.size() != b.size()) return false;
    const auto less = [](const TopologyEntry& x, const TopologyEntry& y) { return key_of(x) < key_of(y); };
    Topology sa = a;
    Topology sb = b;
    std::sort(sa.begin(), sa.end(), less);
    std::sort(sb.begin(), sb.end(), less);
    return sa == sb;
}

core::Result<void> check_supported(const transport::DiscoveryResponsePayload& discovery) {
    for (const auto& d : discovery.devices) {
        for (const auto& e : d.endpoints) {
            if (e.capabilities.size() != 1) return core::Result<void>::failure(unsupported("an endpoint must advertise exactly one capability"));
            const std::uint64_t id = e.capabilities.front().id;
            const bool sensor_id = id == capability_id_of(EndpointKind::ACCELERATION) || id == capability_id_of(EndpointKind::ANGULAR_VELOCITY) ||
                                   id == capability_id_of(EndpointKind::POSITION);
            const bool actuator_id = id == capability_id_of(EndpointKind::MOTOR_COMMAND);
            const bool ok = e.direction == transport::WireDirection::SENSOR ? sensor_id : actuator_id;
            if (!ok) return core::Result<void>::failure(unsupported("an endpoint advertises an unsupported capability for its direction"));
        }
    }
    return core::Result<void>::success();
}

} // namespace kritva::hardware::remote
