//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : edge_diagnostics.hpp
// Description : Read-only diagnostics of the Edge side: a snapshot of an EdgeHost, its session and the endpoints it serves.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: DR-001; DR-002; DR-003
// API         : kritva::hardware::remote::EdgeDiagnostics / diagnose / describe
//
// Author      : KritvaOS
// Created     : 06-10-2026
//==============================================================================

#pragma once

#include <string>
#include <vector>

#include <kritva/hardware/diagnostics.hpp>
#include <kritva/hardware/remote/edge_host.hpp>

namespace kritva::hardware::remote {

/// A coherent copy of what the Edge knows: it is the authority for the actuators' state. This is the same-process
/// diagnostic side of the simulation: the Nexus never reads it (RemoteNode has no reference to an EdgeHost).
struct EdgeDiagnostics {
    NodeId node;
    SessionId session;                        ///< invalid when there is no session
    transport::SessionState link_state{transport::SessionState::DISCONNECTED};
    transport::LinkTiming timing;             ///< the heartbeat timing accepted in the current HELLO; zeros without a session
    std::uint64_t now_ns{0};
    std::uint64_t last_valid_frame_ns{0};
    bool sealed{false};
    EdgeStats stats;
    std::vector<DeviceDiagnostics> devices;   ///< the I3 snapshots of the served devices and endpoints (the real Edge state)
};

/// PASSIVE, like the Nexus snapshot: it calls neither poll() nor anything that handles a frame, stops, starts or faults an
/// endpoint, resets a counter or touches the session. It reads the same I3 state a DeviceManager would.
[[nodiscard]] EdgeDiagnostics diagnose(const EdgeHost& edge);

/// Deterministic, sanitized text of an Edge snapshot (see describe() of the Nexus snapshot for the rules).
[[nodiscard]] std::string describe(const EdgeDiagnostics& diagnostics);

} // namespace kritva::hardware::remote
