//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_session.cpp
// Description : The Nexus-side protocol session.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: RR-002; PR-003; FRL-002 (mechanism); NDR-003
// API         : kritva::hardware::remote::RemoteSession
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <kritva/hardware/remote/remote_session.hpp>

#include <algorithm>

namespace kritva::hardware::remote {

using namespace transport;
using core::ErrorCode;

namespace {

constexpr std::size_t kTimedOutMemory = 64;
constexpr std::uint64_t kNsPerMs = 1'000'000;

using Bytes = std::vector<std::uint8_t>;

core::Error make_error(ErrorCode code, std::string message) {
    return core::Error{code, core::ErrorSeverity::ERROR, {}, {}, std::move(message)};
}

core::Error error_from(const StatusField& status) {
    return make_error(status.code, status.message.empty() ? std::string("the Edge reported an error") : status.message);
}

std::uint64_t saturating_add(std::uint64_t a, std::uint64_t b) noexcept {
    return a > UINT64_MAX - b ? UINT64_MAX : a + b;
}

template <class T>
std::function<bool(ByteSpan)> decodes_as() {
    return [](ByteSpan p) { return decode<T>(p).ok(); };
}

} // namespace

RemoteSession::RemoteSession(Transport& link, RemoteSessionConfig config) : link_(link), config_(std::move(config)) {}

// ---- state --------------------------------------------------------------------------------------------------

void RemoteSession::to_state(SessionState next) noexcept {
    if (is_valid_transition(state_, next)) state_ = next;     // an invalid transition changes nothing
}

void RemoteSession::disconnect_session() noexcept {
    to_state(SessionState::DISCONNECTED);
    session_ = 0;
    outstanding_.clear();
}

void RemoteSession::close() { end_session(LinkLossKind::SESSION_CLOSED); }

void RemoteSession::set_handlers(LinkLostHandler link_lost, FaultEventHandler fault_event) {
    link_lost_ = std::move(link_lost);
    fault_event_ = std::move(fault_event);
}

// A live session ends as one transition: it is DISCONNECTED (and its id forgotten) before anyone is told, once.
void RemoteSession::end_session(LinkLossKind kind) {
    const bool live = state_ == SessionState::CONNECTED || state_ == SessionState::DEGRADED;
    const std::uint64_t id = session_;
    disconnect_session();
    if (!live) return;                                       // a handshake that fails is not a link loss
    ++stats_.sessions_lost;
    if (link_lost_) link_lost_(LinkLoss{kind, reason_of(kind), id, link_.now_ns()});
}

// Any valid in-session frame from the Edge is proof of liveness; it also ends DEGRADED.
void RemoteSession::refresh_liveness() {
    last_valid_frame_ns_ = link_.now_ns();
    if (state_ == SessionState::DEGRADED) to_state(SessionState::CONNECTED);
}

void RemoteSession::send_heartbeat() {
    const auto payload = encode(HeartbeatPayload{link_.now_ns()});
    FrameHeader header;
    header.type = MessageType::HEARTBEAT;
    header.sequence = ++out_sequence_;
    header.correlation_id = 0;
    header.session_id = session_;
    const auto frame = payload ? encode_frame(header, payload.value()) : core::Result<Bytes>::failure(payload.error());
    if (!frame || !link_.send(frame.value())) { ++stats_.heartbeat_send_failures; return; }
    ++stats_.heartbeats_sent;
}

void RemoteSession::service() {
    if (state_ != SessionState::CONNECTED && state_ != SessionState::DEGRADED) return;
    if (link_.link_state() != LinkState::CONNECTED) { end_session(LinkLossKind::LINK_LOST); return; }
    // What has become due is read first, so that a heartbeat or a FAULT_EVENT counts before the supervision decides. A frame's
    // liveness time is the virtual time at which it is handled: a backlog is handled when the Nexus is driven again (a gap in
    // driving is a gap in supervision).
    while (auto incoming = link_.receive()) handle(*incoming);
    if (state_ != SessionState::CONNECTED && state_ != SessionState::DEGRADED) return;
    const std::uint64_t now = link_.now_ns();
    const std::uint64_t period_ns = static_cast<std::uint64_t>(config_.timing.heartbeat_period_ms) * kNsPerMs;
    const std::uint64_t timeout_ns = static_cast<std::uint64_t>(config_.timing.heartbeat_timeout_ms) * kNsPerMs;
    const std::uint64_t since = now - last_valid_frame_ns_;
    if (since >= timeout_ns) { end_session(LinkLossKind::LINK_LOST); return; }          // exactly at the timeout, not only after it
    if (since >= 2 * period_ns) {
        if (state_ == SessionState::CONNECTED) { to_state(SessionState::DEGRADED); ++stats_.degraded_entries; }
    } else if (state_ == SessionState::DEGRADED) {
        to_state(SessionState::CONNECTED);
    }
    if (now >= next_heartbeat_ns_) {                                        // at most one per call: a jump in time is not a burst
        send_heartbeat();
        next_heartbeat_ns_ = saturating_add(now, period_ns);
    }
}

core::Result<void> RemoteSession::require_connected() const {
    if (state_ != SessionState::CONNECTED) {
        return core::Result<void>::failure(make_error(ErrorCode::RESOURCE_UNAVAILABLE, "the remote session is not connected"));
    }
    return core::Result<void>::success();
}

// ---- HELLO and discovery -------------------------------------------------------------------------------------

core::Result<void> RemoteSession::open() {
    if (registered_) return core::Result<void>::failure(make_error(ErrorCode::INVALID_STATE, "the session is already open; use reopen()"));
    return handshake(false);
}

core::Result<void> RemoteSession::reopen() {
    if (!registered_) return core::Result<void>::failure(make_error(ErrorCode::INVALID_STATE, "the session was never opened"));
    return handshake(true);
}

core::Result<void> RemoteSession::handshake(bool compare) {
    if (!config_.nexus_node.valid() || !config_.edge_node.valid() || !is_valid(config_.timing)) {
        return core::Result<void>::failure(make_error(ErrorCode::INVALID_ARGUMENT, "the node ids and the link timing must be valid"));
    }
    disconnect_session();                                    // whatever session existed is abandoned (the Edge replaces it on HELLO)
    out_sequence_ = 0;
    in_watermark_ = 0;
    hello_session_ = 0;
    if (link_.link_state() != LinkState::CONNECTED) {
        if (auto r = link_.connect(); !r) return r;
    }
    to_state(SessionState::CONNECTING);

    HelloPayload hello;
    hello.node_id = config_.nexus_node.value();
    hello.major = kProtocolMajor;
    hello.minor = kProtocolMinor;
    hello.heartbeat_period_ms = config_.timing.heartbeat_period_ms;
    hello.heartbeat_timeout_ms = config_.timing.heartbeat_timeout_ms;
    const auto hello_bytes = encode(hello);
    if (!hello_bytes) { disconnect_session(); return core::Result<void>::failure(hello_bytes.error()); }

    auto reply = exchange(MessageType::HELLO, ++out_sequence_, hello_bytes.value(), true, decodes_as<HelloAckPayload>());
    if (!reply) { disconnect_session(); return core::Result<void>::failure(reply.error()); }
    const HelloAckPayload ack = decode<HelloAckPayload>(reply.value()).value;
    if (!ack.status.ok()) { disconnect_session(); return core::Result<void>::failure(error_from(ack.status)); }
    if (ack.node_id != config_.edge_node.value() || ack.session_id != hello_session_ || ack.major != hello.major || ack.minor != hello.minor ||
        ack.heartbeat_period_ms != hello.heartbeat_period_ms || ack.heartbeat_timeout_ms != hello.heartbeat_timeout_ms) {
        disconnect_session();
        return core::Result<void>::failure(make_error(ErrorCode::INVALID_ARGUMENT, "the HELLO_ACK does not match the HELLO or the expected Edge"));
    }
    session_ = ack.session_id;
    last_valid_frame_ns_ = link_.now_ns();
    to_state(SessionState::NEGOTIATING);

    auto discovered = exchange(MessageType::DISCOVERY_REQUEST, ++out_sequence_, {}, false, decodes_as<DiscoveryResponsePayload>());
    if (!discovered) { disconnect_session(); return core::Result<void>::failure(discovered.error()); }
    DiscoveryResponsePayload discovery = decode<DiscoveryResponsePayload>(discovered.value()).value;
    if (!discovery.status.ok()) { disconnect_session(); return core::Result<void>::failure(error_from(discovery.status)); }
    if (auto supported = check_supported(discovery); !supported) { disconnect_session(); return supported; }

    const Topology topology = topology_of(config_.edge_node, discovery);
    if (compare) {
        if (!topology_equivalent(topology, registered_topology_)) {
            disconnect_session();
            return core::Result<void>::failure(make_error(ErrorCode::CONFIGURATION_ERROR, "the Edge topology changed since it was registered"));
        }
    } else {
        registered_ = true;
        registered_topology_ = topology;
        discovery_ = std::move(discovery);
    }
    last_valid_frame_ns_ = link_.now_ns();
    next_heartbeat_ns_ = saturating_add(last_valid_frame_ns_, static_cast<std::uint64_t>(config_.timing.heartbeat_period_ms) * kNsPerMs);
    to_state(SessionState::CONNECTED);
    return core::Result<void>::success();
}

// ---- the exchange ---------------------------------------------------------------------------------------------

core::Result<Bytes> RemoteSession::exchange(MessageType type, std::uint64_t sequence, ByteSpan payload, bool hello,
                                            std::function<bool(ByteSpan)> well_formed) {
    FrameHeader header;
    header.type = type;
    header.sequence = sequence;
    header.correlation_id = 0;
    header.session_id = hello ? 0 : session_;
    const auto frame = encode_frame(header, payload);
    if (!frame) return core::Result<Bytes>::failure(frame.error());
    if (auto sent = link_.send(frame.value()); !sent) {
        if (link_.link_state() != LinkState::CONNECTED) end_session(LinkLossKind::LINK_LOST);
        return core::Result<Bytes>::failure(sent.error());
    }
    ++stats_.requests_sent;
    timed_out_.erase(std::remove(timed_out_.begin(), timed_out_.end(), sequence), timed_out_.end());   // a retransmission is answered again

    Outstanding o;
    o.expected = *response_for(type);
    o.deadline_ns = saturating_add(link_.now_ns(), static_cast<std::uint64_t>(config_.timing.request_timeout_ms) * kNsPerMs);
    o.hello = hello;
    o.well_formed = std::move(well_formed);
    const std::uint64_t deadline = o.deadline_ns;
    outstanding_[sequence] = std::move(o);
    const std::uint64_t quantum = static_cast<std::uint64_t>(config_.timing.pump_quantum_ms) * kNsPerMs;

    for (;;) {
        if (config_.peer_tick) config_.peer_tick();            // the peer handles what is due now
        while (auto incoming = link_.receive()) handle(*incoming);

        const auto it = outstanding_.find(sequence);
        if (it == outstanding_.end()) {                          // the session was torn down under the call
            return core::Result<Bytes>::failure(make_error(ErrorCode::RESOURCE_UNAVAILABLE, "the remote session ended"));
        }
        if (it->second.done) {
            Outstanding finished = std::move(it->second);
            outstanding_.erase(it);
            if (finished.ok) return core::Result<Bytes>::success(std::move(finished.payload));
            return core::Result<Bytes>::failure(finished.error);
        }
        if (link_.link_state() != LinkState::CONNECTED) {
            outstanding_.erase(sequence);
            end_session(LinkLossKind::LINK_LOST);
            return core::Result<Bytes>::failure(make_error(ErrorCode::RESOURCE_UNAVAILABLE, "the link is down"));
        }
        service();                                               // supervision at every pump step: heartbeat, DEGRADED, timeout
        const std::uint64_t now = link_.now_ns();
        if (now >= deadline) {                                   // a response due exactly at the deadline was handled above
            outstanding_.erase(sequence);
            ++stats_.timeouts;
            timed_out_.push_back(sequence);
            if (timed_out_.size() > kTimedOutMemory) timed_out_.pop_front();
            return core::Result<Bytes>::failure(make_error(ErrorCode::TIMEOUT, "the request timed out"));
        }
        if (auto advanced = link_.advance(std::min(quantum, deadline - now)); !advanced) {
            outstanding_.erase(sequence);
            return core::Result<Bytes>::failure(advanced.error());
        }
    }
}

core::Result<Bytes> RemoteSession::request(MessageType type, ByteSpan payload, std::function<bool(ByteSpan)> well_formed) {
    if (auto ok = require_connected(); !ok) return core::Result<Bytes>::failure(ok.error());
    return exchange(type, ++out_sequence_, payload, false, std::move(well_formed));
}

// ---- incoming frames ----------------------------------------------------------------------------------------------

void RemoteSession::handle(const Bytes& bytes) {
    const DecodedFrame frame = decode_frame(bytes, Receiver::NEXUS);
    if (!frame.ok()) { ++stats_.frame_errors; return; }
    if (is_response_like(frame.header.type, frame.header.correlation_id)) handle_response(frame.header, frame.payload);
    else handle_notice(frame.header, frame.payload);
}

void RemoteSession::handle_response(const FrameHeader& header, ByteSpan payload) {
    // Admission is by correlation, never by the sequence watermark: a response may follow a later heartbeat.
    const auto it = outstanding_.find(header.correlation_id);
    if (it == outstanding_.end()) {
        if (std::find(timed_out_.begin(), timed_out_.end(), header.correlation_id) != timed_out_.end()) ++stats_.late_responses;
        else ++stats_.unknown_correlation;
        return;
    }
    Outstanding& o = it->second;
    if (o.done) { ++stats_.unknown_correlation; return; }       // a duplicate of an answer already taken
    if (!o.hello && (session_ == 0 || header.session_id != session_)) { ++stats_.wrong_session; return; }

    if (header.type == MessageType::PROTOCOL_ERROR) {           // the Edge refused the request's payload: it fails now
        const auto err = decode_protocol_error(payload);
        o.done = true;
        o.ok = false;
        if (err.ok()) { o.error = error_from(err.value.status); refresh_liveness(); }
        else { ++stats_.malformed_responses; o.error = make_error(ErrorCode::INVALID_ARGUMENT, "malformed PROTOCOL_ERROR"); }
        if (o.hello) hello_session_ = header.session_id;
        return;
    }
    if (header.type != o.expected) { ++stats_.wrong_type; return; }
    if (!o.well_formed(payload)) {
        ++stats_.malformed_responses;
        o.done = true;
        o.ok = false;
        o.error = make_error(ErrorCode::INVALID_ARGUMENT, "malformed response");
        return;
    }
    if (o.hello) hello_session_ = header.session_id;
    o.done = true;
    o.ok = true;
    o.payload.assign(payload.begin(), payload.end());
    ++stats_.responses_accepted;
    refresh_liveness();
}

void RemoteSession::handle_notice(const FrameHeader& header, ByteSpan payload) {
    if (session_ == 0 || header.session_id != session_) { ++stats_.wrong_session; return; }
    // The payload is decoded before the frame is admitted: a malformed notice must not raise the watermark or count as liveness.
    std::optional<FaultEventPayload> fault;
    switch (header.type) {
        case MessageType::HEARTBEAT:
            if (!decode<HeartbeatPayload>(payload).ok()) { ++stats_.malformed_notices; return; }
            break;
        case MessageType::FAULT_EVENT: {
            auto event = decode<FaultEventPayload>(payload);
            if (!event.ok()) { ++stats_.malformed_notices; return; }
            fault = std::move(event.value);
            break;
        }
        case MessageType::PROTOCOL_ERROR:
            if (!decode_protocol_error(payload).ok()) { ++stats_.malformed_notices; return; }
            break;
        default:
            ++stats_.frame_errors;
            return;
    }
    if (header.sequence <= in_watermark_) { ++stats_.stale_notices; return; }
    in_watermark_ = header.sequence;
    refresh_liveness();
    ++stats_.notices;
    if (!fault) return;
    if (fault->address.node_id != config_.edge_node.value()) { ++stats_.wrong_node_events; return; }
    ++stats_.fault_events;
    if (fault_event_) {
        fault_event_(EndpointAddress{config_.edge_node, DeviceId{fault->address.device_id}, EndpointId{fault->address.endpoint_id}}, fault->reason);
    }
}

// ---- typed requests --------------------------------------------------------------------------------------------------

namespace {

Bytes address_bytes(NodeId node, const EndpointAddress& a) {
    const auto encoded = encode(AddressPayload{node.value(), a.device.value(), a.endpoint.value()});
    return encoded ? encoded.value() : Bytes{};
}

}  // namespace

core::Result<LifecycleResponsePayload> RemoteSession::lifecycle(MessageType type, const EndpointAddress& address) {
    const auto reply = request(type, address_bytes(config_.edge_node, address), decodes_as<LifecycleResponsePayload>());
    if (!reply) return core::Result<LifecycleResponsePayload>::failure(reply.error());
    return core::Result<LifecycleResponsePayload>::success(decode<LifecycleResponsePayload>(reply.value()).value);
}

core::Result<LifecycleResponsePayload> RemoteSession::configure(const EndpointAddress& address, std::vector<ConfigureSetting> settings) {
    const auto payload = encode(ConfigureRequestPayload{AddressPayload{config_.edge_node.value(), address.device.value(), address.endpoint.value()}, std::move(settings)});
    if (!payload) return core::Result<LifecycleResponsePayload>::failure(payload.error());
    const auto reply = request(MessageType::CONFIGURE_REQUEST, payload.value(), decodes_as<LifecycleResponsePayload>());
    if (!reply) return core::Result<LifecycleResponsePayload>::failure(reply.error());
    return core::Result<LifecycleResponsePayload>::success(decode<LifecycleResponsePayload>(reply.value()).value);
}

core::Result<ReadResponsePayload> RemoteSession::read(const EndpointAddress& address, ReadKind kind) {
    const auto reply = request(MessageType::READ_REQUEST, address_bytes(config_.edge_node, address),
                               [kind](ByteSpan p) { return decode_read_response(p, kind).ok(); });
    if (!reply) return core::Result<ReadResponsePayload>::failure(reply.error());
    return core::Result<ReadResponsePayload>::success(decode_read_response(reply.value(), kind).value);
}

core::Result<ObserveResponsePayload> RemoteSession::observe(const EndpointAddress& address) {
    const auto reply = request(MessageType::OBSERVE_REQUEST, address_bytes(config_.edge_node, address), decodes_as<ObserveResponsePayload>());
    if (!reply) return core::Result<ObserveResponsePayload>::failure(reply.error());
    return core::Result<ObserveResponsePayload>::success(decode<ObserveResponsePayload>(reply.value()).value);
}

core::Result<StatusPayload> RemoteSession::write(const EndpointAddress& address, double velocity_rad_s) {
    if (auto ok = require_connected(); !ok) return core::Result<StatusPayload>::failure(ok.error());
    const auto payload = encode(WriteRequestPayload{AddressPayload{config_.edge_node.value(), address.device.value(), address.endpoint.value()}, velocity_rad_s});
    if (!payload) return core::Result<StatusPayload>::failure(payload.error());          // for example a non-finite velocity
    const std::uint64_t sequence = ++out_sequence_;
    writes_[{address.device.value(), address.endpoint.value()}] = SentWrite{session_, sequence, payload.value()};
    const auto reply = exchange(MessageType::WRITE_REQUEST, sequence, payload.value(), false, decodes_as<StatusPayload>());
    if (!reply) return core::Result<StatusPayload>::failure(reply.error());
    return core::Result<StatusPayload>::success(decode<StatusPayload>(reply.value()).value);
}

core::Result<StatusPayload> RemoteSession::retransmit_write(const EndpointAddress& address) {
    if (auto ok = require_connected(); !ok) return core::Result<StatusPayload>::failure(ok.error());
    const auto it = writes_.find({address.device.value(), address.endpoint.value()});
    if (it == writes_.end() || it->second.session != session_) {
        return core::Result<StatusPayload>::failure(make_error(ErrorCode::INVALID_STATE, "there is no write of the current session to retransmit"));
    }
    ++stats_.retransmissions;
    const SentWrite sent = it->second;                           // the SAME sequence and payload
    const auto reply = exchange(MessageType::WRITE_REQUEST, sent.sequence, sent.payload, false, decodes_as<StatusPayload>());
    if (!reply) return core::Result<StatusPayload>::failure(reply.error());
    return core::Result<StatusPayload>::success(decode<StatusPayload>(reply.value()).value);
}

std::optional<std::uint64_t> RemoteSession::last_write_sequence(const EndpointAddress& address) const {
    const auto it = writes_.find({address.device.value(), address.endpoint.value()});
    if (it == writes_.end() || it->second.session != session_) return std::nullopt;
    return it->second.sequence;
}

} // namespace kritva::hardware::remote
