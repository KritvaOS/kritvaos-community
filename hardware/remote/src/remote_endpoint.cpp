//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_endpoint.cpp
// Description : Nexus-side proxies of Edge endpoints.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: RR-001; RR-002; RR-003; SR-001
// API         : kritva::hardware::remote::RemoteAccelerationEndpoint and the other proxies
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <kritva/hardware/remote/remote_endpoint.hpp>

#include <variant>

namespace kritva::hardware::remote {

using namespace transport;
using core::ErrorCode;

core::Error error_of(const StatusField& status) {
    return core::Error{status.code, core::ErrorSeverity::ERROR, {}, {}, status.message};
}

namespace {

core::Error local_error(ErrorCode code, const char* message) {
    return core::Error{code, core::ErrorSeverity::ERROR, {}, {}, message};
}

core::Timestamp stamp(std::int64_t ns) { return core::Timestamp{ns, core::ClockDomain::MONOTONIC}; }

template <class Sample> constexpr ReadKind read_kind_of() { return ReadKind::VEC3; }
template <> constexpr ReadKind read_kind_of<PositionSample>() { return ReadKind::SCALAR; }

template <class Sample> Sample to_sample(const ReadResponsePayload& p);
template <> AccelerationSample to_sample<AccelerationSample>(const ReadResponsePayload& p) {
    return AccelerationSample{Vec3{p.x, p.y, p.z}, p.sample_sequence, stamp(p.timestamp_ns)};
}
template <> AngularVelocitySample to_sample<AngularVelocitySample>(const ReadResponsePayload& p) {
    return AngularVelocitySample{Vec3{p.x, p.y, p.z}, p.sample_sequence, stamp(p.timestamp_ns)};
}
template <> PositionSample to_sample<PositionSample>(const ReadResponsePayload& p) {
    return PositionSample{p.value, p.sample_sequence, stamp(p.timestamp_ns)};
}

} // namespace

// ---- the shared lifecycle mapping ------------------------------------------------------------------------------

core::Result<void> RemoteProxy::configure(const core::Configuration& scoped) {
    std::vector<ConfigureSetting> settings;
    for (const std::string& name : settings_) {
        const core::Parameter* p = scoped.get(name);
        if (p == nullptr) continue;
        if (const auto* b = std::get_if<bool>(&p->value)) settings.push_back({name, *b});
        else if (const auto* i = std::get_if<std::int64_t>(&p->value)) settings.push_back({name, *i});
        else if (const auto* s = std::get_if<std::string>(&p->value)) settings.push_back({name, *s});
        else return core::Result<void>::failure(local_error(ErrorCode::INVALID_ARGUMENT, "floating-point settings cannot be sent by protocol 1.0"));
    }
    auto r = node_.session().configure(address_, std::move(settings));
    if (!r) return core::Result<void>::failure(r.error());
    if (!r.value().status.ok()) return core::Result<void>::failure(error_of(r.value().status));
    return core::Result<void>::success();
}

core::Result<void> RemoteProxy::lifecycle(MessageType request) {
    auto r = node_.session().lifecycle(request, address_);
    if (!r) return core::Result<void>::failure(r.error());
    if (!r.value().status.ok()) return core::Result<void>::failure(error_of(r.value().status));
    return core::Result<void>::success();
}

core::Result<void> RemoteProxy::initialize() {
    if (!holds_period_) {                                     // the first endpoint of a live period establishes a fresh session
        if (auto acquired = node_.acquire(); !acquired) return acquired;
        holds_period_ = true;
    }
    if (cleanup_pending_) {
        // The last shutdown was local only (the link was gone), so the Edge endpoint may still be in any state, for
        // example a sensor that kept running. Bring it to a clean STOPPED state first: STOP is best effort (it fails
        // harmlessly in a state that cannot stop), SHUTDOWN must succeed. Then the I3 initialize can start from STOPPED.
        (void)lifecycle(MessageType::STOP_REQUEST);
        if (auto cleaned = lifecycle(MessageType::SHUTDOWN_REQUEST); !cleaned) { node_.release(); holds_period_ = false; return cleaned; }
        cleanup_pending_ = false;
    }
    auto r = lifecycle(MessageType::INITIALIZE_REQUEST);
    if (!r) { node_.release(); holds_period_ = false; }
    return r;
}

core::Result<void> RemoteProxy::start() { return lifecycle(MessageType::START_REQUEST); }
core::Result<void> RemoteProxy::stop() { return lifecycle(MessageType::STOP_REQUEST); }

core::Result<void> RemoteProxy::shutdown() {
    core::Result<void> r = core::Result<void>::success();
    // With the link gone there is nobody to tell: the local endpoint is released (link loss is I4-006's policy).
    if (node_.session().state() == SessionState::CONNECTED) r = lifecycle(MessageType::SHUTDOWN_REQUEST);
    else cleanup_pending_ = true;                         // the Edge endpoint is cleaned up by the next initialize
    if (r && holds_period_) { node_.release(); holds_period_ = false; }
    return r;
}

// ---- sensors ----------------------------------------------------------------------------------------------------

template <class Base, class Sample>
core::Result<void> RemoteSensorEndpoint<Base, Sample>::do_read(Sample& out) {
    auto r = proxy_.node().session().read(proxy_.address(), read_kind_of<Sample>());
    if (!r) return core::Result<void>::failure(r.error());
    if (!r.value().status.ok()) return core::Result<void>::failure(error_of(r.value().status));
    out = to_sample<Sample>(r.value());
    return core::Result<void>::success();
}

template class RemoteSensorEndpoint<AccelerationEndpoint, AccelerationSample>;
template class RemoteSensorEndpoint<AngularVelocityEndpoint, AngularVelocitySample>;
template class RemoteSensorEndpoint<PositionEndpoint, PositionSample>;

// ---- the actuator -------------------------------------------------------------------------------------------------

namespace {

core::Error write_error(const core::Error& e) {
    if (e.code == ErrorCode::TIMEOUT) return local_error(ErrorCode::TIMEOUT, "the write timed out: its outcome on the Edge is unknown");
    return e;
}

} // namespace

core::Result<void> RemoteMotorCommandEndpoint::do_write(const MotorCommand& command) {
    auto r = proxy_.node().session().write(proxy_.address(), command.velocity_rad_s);
    if (!r) return core::Result<void>::failure(write_error(r.error()));
    if (!r.value().status.ok()) return core::Result<void>::failure(error_of(r.value().status));
    return core::Result<void>::success();
}

core::Result<void> RemoteMotorCommandEndpoint::retransmit_last_write() {
    auto result = check_operational("retransmit_last_write");
    if (result) {
        auto r = proxy_.node().session().retransmit_write(proxy_.address());
        if (!r) result = core::Result<void>::failure(write_error(r.error()));
        else if (!r.value().status.ok()) result = core::Result<void>::failure(error_of(r.value().status));
    }
    count_operation(result.has_value());
    if (!result) note_error(result.error());
    return result;
}

} // namespace kritva::hardware::remote
