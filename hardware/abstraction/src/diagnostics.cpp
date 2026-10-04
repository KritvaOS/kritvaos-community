//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : diagnostics.cpp
// Description : Diagnostics text rendering.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Diagnostics
// Layer       : Hardware Abstraction
//
// Requirements: DER-601..608; DER-702
// API         : kritva::hardware::describe
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <kritva/hardware/diagnostics.hpp>

namespace kritva::hardware {

namespace {

const char* name_of(core::LifecycleState s) {
    switch (s) {
        case core::LifecycleState::UNKNOWN: return "UNKNOWN";           case core::LifecycleState::INITIALIZING: return "INITIALIZING";
        case core::LifecycleState::READY: return "READY";               case core::LifecycleState::RUNNING: return "RUNNING";
        case core::LifecycleState::STOPPING: return "STOPPING";         case core::LifecycleState::STOPPED: return "STOPPED";
        case core::LifecycleState::FAULT: return "FAULT";               case core::LifecycleState::RECOVERING: return "RECOVERING";
    }
    return "INVALID";
}

const char* name_of(core::StatusCode s) {
    switch (s) {
        case core::StatusCode::UNKNOWN: return "UNKNOWN";               case core::StatusCode::OK: return "OK";
        case core::StatusCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT"; case core::StatusCode::NOT_READY: return "NOT_READY";
        case core::StatusCode::BUSY: return "BUSY";                     case core::StatusCode::TIMEOUT: return "TIMEOUT";
        case core::StatusCode::FAILED: return "FAILED";                 case core::StatusCode::UNAVAILABLE: return "UNAVAILABLE";
        case core::StatusCode::UNSUPPORTED: return "UNSUPPORTED";       case core::StatusCode::INTERNAL_ERROR: return "INTERNAL_ERROR";
    }
    return "INVALID";
}

const char* name_of(core::HealthState s) {
    switch (s) {
        case core::HealthState::UNKNOWN: return "UNKNOWN";   case core::HealthState::HEALTHY: return "HEALTHY";
        case core::HealthState::DEGRADED: return "DEGRADED"; case core::HealthState::UNHEALTHY: return "UNHEALTHY";
    }
    return "INVALID";
}

const char* name_of(core::ErrorCode c) {
    switch (c) {
        case core::ErrorCode::NONE: return "NONE";                       case core::ErrorCode::UNKNOWN: return "UNKNOWN";
        case core::ErrorCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT"; case core::ErrorCode::INVALID_STATE: return "INVALID_STATE";
        case core::ErrorCode::NOT_INITIALIZED: return "NOT_INITIALIZED"; case core::ErrorCode::NOT_READY: return "NOT_READY";
        case core::ErrorCode::ALREADY_RUNNING: return "ALREADY_RUNNING"; case core::ErrorCode::TIMEOUT: return "TIMEOUT";
        case core::ErrorCode::RESOURCE_UNAVAILABLE: return "RESOURCE_UNAVAILABLE";
        case core::ErrorCode::CONFIGURATION_ERROR: return "CONFIGURATION_ERROR"; case core::ErrorCode::UNSUPPORTED: return "UNSUPPORTED";
        case core::ErrorCode::INTERNAL_ERROR: return "INTERNAL_ERROR";
    }
    return "INVALID";
}

// Text that comes from a device or an error can neither split a line nor close a quoted
// field: control characters, the double quote and the comma-list separator are replaced.
std::string clean(const std::string& text) {
    std::string out = text;
    for (char& c : out) if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f || c == '"') c = '?';
    return out;
}

std::string join(const std::vector<std::string>& items) {
    std::string out;
    for (const auto& i : items) {
        std::string item = clean(i);
        for (char& c : item) if (c == ',') c = '?';
        out += (out.empty() ? "" : ",") + item;
    }
    return out.empty() ? "none" : out;
}

} // namespace

std::string describe(const std::vector<DeviceDiagnostics>& devices) {
    std::string out;
    for (const auto& d : devices) {
        out += "device " + d.name + " (id=" + std::to_string(d.id.value()) + ") enabled=" + (d.enabled ? "true" : "false") +
               " status=" + name_of(d.status) + " health=" + name_of(d.health);
        if (!d.health_detail.empty()) out += " detail=\"" + clean(d.health_detail) + "\"";
        out += " capabilities=" + join(d.capabilities) + "\n";
        for (const auto& e : d.endpoints) {
            out += "  endpoint " + e.endpoint_name + " (id=" + std::to_string(e.endpoint_id.value()) + ") " +
                   (e.direction == EndpointDirection::SENSOR ? "sensor" : "actuator") + " state=" + name_of(e.lifecycle) +
                   " status=" + name_of(e.status) + " health=" + name_of(e.health);
            if (!e.health_detail.empty()) out += " detail=\"" + clean(e.health_detail) + "\"";
            out += " ok=" + std::to_string(e.operations_ok) + " failed=" + std::to_string(e.operations_failed) +
                   " capabilities=" + join(e.capabilities);
            if (e.last_error) out += std::string(" last_error=") + name_of(e.last_error->code) + " \"" + clean(e.last_error->message) + "\"";
            out += "\n";
        }
    }
    return out;
}

} // namespace kritva::hardware
