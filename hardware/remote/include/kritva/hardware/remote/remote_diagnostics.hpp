//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_diagnostics.hpp
// Description : Read-only diagnostics of the Nexus side: a snapshot of a RemoteNode, its link and its remote endpoints.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: DR-001; DR-002; DR-003
// API         : kritva::hardware::remote::RemoteNodeDiagnostics / diagnose / describe
//
// Author      : KritvaOS
// Created     : 06-10-2026
//==============================================================================

#pragma once

#include <string>
#include <vector>

#include <kritva/hardware/diagnostics.hpp>
#include <kritva/hardware/remote/remote_node.hpp>

namespace kritva::hardware::remote {

/// One remote endpoint as the Nexus represents it: its identity, the I3 snapshot of the proxy (state, status, health and
/// the reason in its detail, statistics, last error) and why it is FAULT, if it is.
struct RemoteEndpointDiagnostics {
    EndpointAddress address;
    EndpointDiagnostics endpoint;
    FaultOrigin fault_origin{FaultOrigin::NONE};
};

/// A coherent, immutable-by-use copy of everything the Nexus knows about one Edge at the moment of the call.
struct RemoteNodeDiagnostics {
    NodeId edge_node;
    SessionId session;                        ///< invalid when there is no session
    transport::SessionState link_state{transport::SessionState::DISCONNECTED};
    transport::LinkTiming timing;
    std::uint64_t now_ns{0};                  ///< the transport's virtual time when the snapshot was taken
    std::uint64_t last_valid_frame_ns{0};
    RemoteStats link_stats;
    RemoteNodeStats node_stats;
    std::vector<LinkLossRecord> link_loss_records;      ///< a copy: nothing in the public API can change or clear the node's own
    std::vector<RemoteEndpointDiagnostics> endpoints;   ///< in the enumeration order of the remote devices
};

/// The snapshot. PASSIVE: it sends no frame, reads no frame, calls neither RemoteNode::service() nor any request, and changes
/// no state, counter, record, watermark, deadline, session or transport state; it cannot refresh liveness, reconnect,
/// restart, clear a FAULT or acknowledge anything. Diagnostics are observation only and never a second safety mechanism.
/// The Edge's own state is not part of it: the Nexus shows its representation (which can legitimately differ, for example
/// the Edge's actuator already STOPPED while the proxy is still RUNNING until the supervision is driven).
[[nodiscard]] RemoteNodeDiagnostics diagnose(const RemoteNode& node);

/// Deterministic text of a snapshot: the same snapshot always renders the same text. It contains identities, states,
/// counters, fault origins and the sanitized reasons and error messages (control characters, DEL and double quotes replaced);
/// no configuration value and no raw frame content is ever included. The reasons stay as they are: "link lost", "session
/// closed" and "remote fault: <reason>".
[[nodiscard]] std::string describe(const RemoteNodeDiagnostics& diagnostics);

} // namespace kritva::hardware::remote
