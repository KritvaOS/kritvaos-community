//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : diagnostics_text.hpp
// Description : Shared helpers of the I4 diagnostics: I3 snapshots of devices and endpoints, and text sanitizing.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: DR-001; DR-002
// API         : kritva::hardware::remote::detail::snapshot_of / clean_text
//
// Author      : KritvaOS
// Created     : 06-10-2026
//==============================================================================

#pragma once

#include <string>
#include <vector>

#include <kritva/hardware/device.hpp>
#include <kritva/hardware/diagnostics.hpp>

namespace kritva::hardware::remote::detail {

/// Text that comes from a device, an error or the peer can neither split a line nor close a quoted field: control
/// characters, DEL and the double quote are replaced by '?'.
[[nodiscard]] inline std::string clean_text(const std::string& text) {
    std::string out = text;
    for (char& c : out) if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f || c == '"') c = '?';
    return out;
}

/// The I3 snapshot of one endpoint, filled exactly as DeviceManager::diagnostics() fills it (a device that is not under a
/// DeviceManager is reported as enabled). Reads only: it calls nothing that changes a state, a counter or a clock.
[[nodiscard]] inline EndpointDiagnostics endpoint_snapshot(const Device& device, const Endpoint& endpoint) {
    const auto names = [](const core::CapabilitySet& set) {
        std::vector<std::string> out;
        for (const auto& c : set.all()) out.push_back(c.name);
        return out;
    };
    EndpointDiagnostics ed;
    ed.device_id = device.info().id();
    ed.device_name = device.info().name();
    ed.endpoint_id = endpoint.info().id();
    ed.endpoint_name = endpoint.info().name();
    ed.direction = endpoint.info().direction();
    ed.enabled = true;
    ed.lifecycle = endpoint.lifecycle_state();
    ed.status = endpoint.status().code();
    const auto health = endpoint.health();
    ed.health = health.state();
    ed.health_detail = health.detail();
    ed.operations_ok = endpoint.statistics().sample_count.value();
    ed.operations_failed = endpoint.statistics().error_count.value();
    ed.last_error = endpoint.last_error();
    ed.capabilities = names(endpoint.capabilities());
    return ed;
}

/// The I3 snapshot of one device and its endpoints in enumeration order.
[[nodiscard]] inline DeviceDiagnostics device_snapshot(const Device& device) {
    DeviceDiagnostics dd;
    dd.id = device.info().id();
    dd.name = device.info().name();
    dd.enabled = true;
    dd.status = device.status().code();
    const auto health = device.health();
    dd.health = health.state();
    dd.health_detail = health.detail();
    const core::CapabilitySet capabilities = device.capabilities();             // a temporary: held here while its list is read
    for (const auto& c : capabilities.all()) dd.capabilities.push_back(c.name);
    for (const Endpoint* e : device.endpoints()) dd.endpoints.push_back(endpoint_snapshot(device, *e));
    return dd;
}

/// A copy whose device and endpoint names are sanitized too. The I3 text sanitizes details, messages and capability names but
/// takes identifiers as validated; a diagnostic snapshot is plain data that anybody can fill, so the names are cleaned here.
[[nodiscard]] inline std::vector<DeviceDiagnostics> sanitized_devices(std::vector<DeviceDiagnostics> devices) {
    for (DeviceDiagnostics& d : devices) {
        d.name = clean_text(d.name);
        for (EndpointDiagnostics& e : d.endpoints) {
            e.device_name = clean_text(e.device_name);
            e.endpoint_name = clean_text(e.endpoint_name);
        }
    }
    return devices;
}

} // namespace kritva::hardware::remote::detail
