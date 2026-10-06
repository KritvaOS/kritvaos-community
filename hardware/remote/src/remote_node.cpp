//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_node.cpp
// Description : One Edge as seen from the Nexus.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: RR-001; RR-002; NDR-003; ER-002
// API         : kritva::hardware::remote::RemoteNode
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <kritva/hardware/remote/remote_node.hpp>

#include <kritva/hardware/remote/capability_dispatch.hpp>
#include <kritva/hardware/remote/remote_endpoint.hpp>

namespace kritva::hardware::remote {

using namespace transport;
using core::ErrorCode;

namespace {

core::Error make_error(ErrorCode code, const char* message) {
    return core::Error{code, core::ErrorSeverity::ERROR, {}, {}, message};
}

} // namespace

RemoteNode::RemoteNode(Transport& link, RemoteSessionConfig config, RemoteSettings settings)
    : session_(link, std::move(config)), settings_(std::move(settings)) {
    session_.set_handlers([this](const LinkLoss& loss) { on_link_lost(loss); },
                          [this](const EndpointAddress& address, const std::string& reason) { on_fault_event(address, reason); });
}

namespace {

bool live(core::LifecycleState s) { return s == core::LifecycleState::READY || s == core::LifecycleState::RUNNING; }

constexpr std::size_t kMaxRecords = 256;

} // namespace

// The session is already DISCONNECTED here. Each live endpoint becomes FAULT exactly once (a faulted, stopped or never
// started endpoint is left alone), so N live endpoints give N faults, N ERROR notifications and one record.
void RemoteNode::on_link_lost(const LinkLoss& loss) {
    const core::Error cause{ErrorCode::RESOURCE_UNAVAILABLE, core::ErrorSeverity::ERROR, {}, {}, loss.reason};
    std::size_t faulted = 0;
    for (RemoteProxy* proxy : proxies_) {
        if (!live(proxy->state())) continue;
        proxy->fault(cause, loss.kind == LinkLossKind::SESSION_CLOSED ? FaultOrigin::SESSION_CLOSED : FaultOrigin::LINK_LOST);
        ++faulted;
    }
    records_.push_back(LinkLossRecord{loss.time_ns, loss.kind, loss.reason, loss.session_id, faulted});
    if (records_.size() > kMaxRecords) records_.erase(records_.begin());
}

void RemoteNode::on_fault_event(const EndpointAddress& address, const std::string& reason) {
    for (RemoteProxy* proxy : proxies_) {
        if (proxy->address().device != address.device || proxy->address().endpoint != address.endpoint) continue;
        if (!live(proxy->state())) { ++node_stats_.fault_events_ignored; return; }      // not live, or already FAULT: no second event
        proxy->fault(core::Error{ErrorCode::INTERNAL_ERROR, core::ErrorSeverity::ERROR, {}, {}, "remote fault: " + reason}, FaultOrigin::REMOTE_FAULT);
        ++node_stats_.remote_faults_applied;
        return;
    }
    ++node_stats_.fault_events_ignored;
}

core::Result<void> RemoteNode::connect() {
    if (connected_once_) return core::Result<void>::failure(make_error(ErrorCode::INVALID_STATE, "the node is already connected"));
    if (auto opened = session_.open(); !opened) return opened;

    const NodeId node = session_.edge_node();
    std::vector<std::unique_ptr<RemoteDevice>> built;
    for (const DiscoveredDevice& d : session_.discovery().devices) {
        auto info = DeviceInfo::create(DeviceId{d.id}, d.name);
        if (!info) { session_.close(); return core::Result<void>::failure(info.error()); }
        auto device = std::make_unique<RemoteDevice>(info.value(), *this);
        for (const DiscoveredEndpoint& e : d.endpoints) {
            const auto direction = e.direction == WireDirection::ACTUATOR ? EndpointDirection::ACTUATOR : EndpointDirection::SENSOR;
            auto endpoint_info = EndpointInfo::create(EndpointId{e.id}, e.name, direction);
            if (!endpoint_info) { session_.close(); return core::Result<void>::failure(endpoint_info.error()); }
            const EndpointAddress address{node, DeviceId{d.id}, EndpointId{e.id}};
            const auto found = settings_.find({d.name, e.name});
            std::vector<std::string> settings = found == settings_.end() ? std::vector<std::string>{} : found->second;
            const std::uint64_t capability = e.capabilities.front().id;     // exactly one: checked by the session
            const core::CapabilitySet caps = capability_set(core::CapabilityId{capability}, e.capabilities.front().name.c_str());
            std::unique_ptr<Endpoint> proxy;
            if (capability == capability_id_of(EndpointKind::ACCELERATION)) {
                proxy = std::make_unique<RemoteAccelerationEndpoint>(endpoint_info.value(), caps, *this, address, std::move(settings));
            } else if (capability == capability_id_of(EndpointKind::ANGULAR_VELOCITY)) {
                proxy = std::make_unique<RemoteAngularVelocityEndpoint>(endpoint_info.value(), caps, *this, address, std::move(settings));
            } else if (capability == capability_id_of(EndpointKind::POSITION)) {
                proxy = std::make_unique<RemotePositionEndpoint>(endpoint_info.value(), caps, *this, address, std::move(settings));
            } else {
                proxy = std::make_unique<RemoteMotorCommandEndpoint>(endpoint_info.value(), *this, address, std::move(settings));
            }
            if (auto added = device->add_endpoint(std::move(proxy)); !added) { session_.close(); return core::Result<void>::failure(added.error()); }
        }
        built.push_back(std::move(device));
    }
    devices_ = std::move(built);
    connected_once_ = true;
    return core::Result<void>::success();
}

core::Result<void> RemoteNode::acquire() {
    if (live_ == 0) {
        if (auto fresh = session_.reopen(); !fresh) return fresh;     // NDR-003: every fresh initialization is a new session
    } else if (session_.state() != SessionState::CONNECTED) {
        return core::Result<void>::failure(make_error(ErrorCode::RESOURCE_UNAVAILABLE, "the remote session is not connected"));
    }
    ++live_;
    return core::Result<void>::success();
}

FaultOrigin RemoteNode::fault_origin(const EndpointAddress& address) const {
    for (const RemoteProxy* proxy : proxies_) {
        if (proxy->address().device == address.device && proxy->address().endpoint == address.endpoint) return proxy->origin();
    }
    return FaultOrigin::NONE;
}

void RemoteNode::release() noexcept {
    if (live_ > 0) --live_;
}

} // namespace kritva::hardware::remote
