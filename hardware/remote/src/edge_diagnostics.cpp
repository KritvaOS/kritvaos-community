//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : edge_diagnostics.cpp
// Description : Read-only diagnostics of the Edge side.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: DR-001; DR-002; DR-003
// API         : kritva::hardware::remote::diagnose / describe
//
// Author      : KritvaOS
// Created     : 06-10-2026
//==============================================================================

#include <kritva/hardware/remote/edge_diagnostics.hpp>

#include <string>

#include <kritva/hardware/remote/diagnostics_text.hpp>

namespace kritva::hardware::remote {

EdgeDiagnostics diagnose(const EdgeHost& edge) {
    EdgeDiagnostics d;
    d.node = edge.node();
    d.session = edge.session();
    d.link_state = edge.link_state();
    d.timing = edge.timing();
    d.now_ns = edge.now_ns();
    d.last_valid_frame_ns = edge.last_valid_frame_ns();
    d.sealed = edge.sealed();
    d.stats = edge.stats();
    for (const Device* device : edge.registry().devices()) d.devices.push_back(detail::device_snapshot(*device));
    return d;
}

std::string describe(const EdgeDiagnostics& d) {
    const auto n = [](std::uint64_t v) { return std::to_string(v); };
    std::string out;
    out += "edge node=" + n(d.node.value()) + " session=" + n(d.session.value()) + " link=" + transport::session_state_name(d.link_state) +
           " now_ns=" + n(d.now_ns) + " last_valid_ns=" + n(d.last_valid_frame_ns) + " sealed=" + (d.sealed ? "true" : "false") +
           " heartbeat_period_ms=" + n(d.timing.heartbeat_period_ms) + " heartbeat_timeout_ms=" + n(d.timing.heartbeat_timeout_ms) + "\n";
    const EdgeStats& s = d.stats;
    out += "edge traffic frames=" + n(s.frames_received) + " frame_errors=" + n(s.frame_errors) + " no_session=" + n(s.no_session) + " wrong_session=" + n(s.wrong_session) +
           " stale=" + n(s.stale_frames) + " unexpected=" + n(s.unexpected_frames) + " protocol_errors=" + n(s.protocol_errors_sent) + " requests_served=" + n(s.requests_served) +
           " heartbeats=" + n(s.heartbeats) + " heartbeats_sent=" + n(s.heartbeats_sent) + " send_failures=" + n(s.send_failures) + "\n";
    out += "edge sessions hellos_accepted=" + n(s.hellos_accepted) + " hellos_rejected=" + n(s.hellos_rejected) + " timed_out=" + n(s.sessions_timed_out) +
           " actuators_stopped_on_new_session=" + n(s.actuators_stopped_on_new_session) + " actuators_stopped_on_timeout=" + n(s.actuators_stopped_on_timeout) +
           " actuator_stop_failures=" + n(s.actuator_stop_failures) + "\n";
    out += "edge writes applied=" + n(s.writes_applied) + " rejected=" + n(s.writes_rejected) + " resent=" + n(s.writes_resent) + " fault_events_sent=" + n(s.fault_events_sent) +
           " fault_events_dropped=" + n(s.fault_events_dropped) + "\n";
    out += hardware::describe(detail::sanitized_devices(d.devices));
    return out;
}

} // namespace kritva::hardware::remote
