//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_diagnostics.cpp
// Description : Read-only diagnostics of the Nexus side.
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

#include <kritva/hardware/remote/remote_diagnostics.hpp>

#include <string>

#include <kritva/hardware/remote/diagnostics_text.hpp>

namespace kritva::hardware::remote {

RemoteNodeDiagnostics diagnose(const RemoteNode& node) {
    const RemoteSession& session = node.session();
    RemoteNodeDiagnostics d;
    d.edge_node = session.edge_node();
    d.session = session.session();
    d.link_state = session.state();
    d.timing = session.timing();
    d.now_ns = session.now_ns();
    d.last_valid_frame_ns = session.last_valid_frame_ns();
    d.link_stats = session.stats();
    d.node_stats = node.node_stats();
    d.link_loss_records = node.link_loss_records();
    for (const auto& device : node.devices()) {
        for (const Endpoint* e : device->endpoints()) {
            RemoteEndpointDiagnostics ed;
            ed.address = EndpointAddress{d.edge_node, device->info().id(), e->info().id()};
            ed.endpoint = detail::endpoint_snapshot(*device, *e);
            ed.fault_origin = node.fault_origin(ed.address);
            d.endpoints.push_back(std::move(ed));
        }
    }
    return d;
}

std::string describe(const RemoteNodeDiagnostics& d) {
    using detail::clean_text;
    const auto n = [](std::uint64_t v) { return std::to_string(v); };
    std::string out;
    out += "nexus node edge=" + n(d.edge_node.value()) + " session=" + n(d.session.value()) + " link=" + transport::session_state_name(d.link_state) +
           " now_ns=" + n(d.now_ns) + " last_valid_ns=" + n(d.last_valid_frame_ns) + " heartbeat_period_ms=" + n(d.timing.heartbeat_period_ms) +
           " heartbeat_timeout_ms=" + n(d.timing.heartbeat_timeout_ms) + " request_timeout_ms=" + n(d.timing.request_timeout_ms) + "\n";
    const RemoteStats& s = d.link_stats;
    out += "nexus link requests=" + n(s.requests_sent) + " responses=" + n(s.responses_accepted) + " timeouts=" + n(s.timeouts) + " late=" + n(s.late_responses) +
           " unknown=" + n(s.unknown_correlation) + " wrong_type=" + n(s.wrong_type) + " wrong_session=" + n(s.wrong_session) + " malformed=" + n(s.malformed_responses) +
           " frame_errors=" + n(s.frame_errors) + " notices=" + n(s.notices) + " stale_notices=" + n(s.stale_notices) + " malformed_notices=" + n(s.malformed_notices) +
           " heartbeats_sent=" + n(s.heartbeats_sent) + " sessions_lost=" + n(s.sessions_lost) + " degraded_entries=" + n(s.degraded_entries) +
           " fault_events=" + n(s.fault_events) + " wrong_node_events=" + n(s.wrong_node_events) + " retransmissions=" + n(s.retransmissions) + "\n";
    out += "nexus faults remote_faults_applied=" + n(d.node_stats.remote_faults_applied) + " fault_events_ignored=" + n(d.node_stats.fault_events_ignored) + "\n";
    std::size_t index = 0;
    for (const LinkLossRecord& r : d.link_loss_records) {
        out += "link loss " + n(++index) + " time_ns=" + n(r.time_ns) + " kind=" + (r.kind == LinkLossKind::SESSION_CLOSED ? "SESSION_CLOSED" : "LINK_LOST") +
               " reason=\"" + clean_text(r.reason) + "\" session=" + n(r.session_id) + " endpoints_faulted=" + n(r.endpoints_faulted) + "\n";
    }
    // The endpoints in the I3 text format, then the origin of each fault episode.
    std::vector<DeviceDiagnostics> devices;
    for (const RemoteEndpointDiagnostics& e : d.endpoints) {
        if (devices.empty() || devices.back().id != e.endpoint.device_id) {
            DeviceDiagnostics dd;
            dd.id = e.endpoint.device_id;
            dd.name = e.endpoint.device_name;
            devices.push_back(std::move(dd));
        }
        devices.back().endpoints.push_back(e.endpoint);
    }
    out += hardware::describe(detail::sanitized_devices(std::move(devices)));
    for (const RemoteEndpointDiagnostics& e : d.endpoints) {
        out += "origin " + clean_text(e.endpoint.device_name) + "." + clean_text(e.endpoint.endpoint_name) + " (node=" + n(e.address.node.value()) +
               " device=" + n(e.address.device.value()) + " endpoint=" + n(e.address.endpoint.value()) + ") fault_origin=" + fault_origin_name(e.fault_origin) + "\n";
    }
    return out;
}

} // namespace kritva::hardware::remote
