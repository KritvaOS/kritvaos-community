//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : edge_host.cpp
// Description : The Edge-side service.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: ER-001; ER-002; ER-003; SR-001; SR-002
// API         : kritva::hardware::remote::EdgeHost
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <kritva/hardware/remote/edge_host.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <variant>

#include <kritva/hardware/remote/capability_dispatch.hpp>

namespace kritva::hardware::remote {

using namespace transport;
using core::ErrorCode;

namespace {

core::Error make_error(ErrorCode code, std::string message) {
    return core::Error{code, core::ErrorSeverity::ERROR, {}, {}, std::move(message)};
}

// Device- or error-supplied text must be valid wire text (<= 256 bytes, strict UTF-8, no control characters).
// Printable ASCII is kept; every other byte becomes '?'; the result is cut at 256 bytes.
std::string wire_text(const std::string& in) {
    std::string out;
    out.reserve(std::min<std::size_t>(in.size(), kMaxTextLength));
    for (const char c : in) {
        if (out.size() == kMaxTextLength) break;
        const auto u = static_cast<unsigned char>(c);
        out.push_back(u >= 0x20 && u <= 0x7E ? c : '?');
    }
    return out;
}

// The wire status of a failing I3 operation: the endpoint's own code and message. TIMEOUT is local to a
// requester (spec section 8) and is never sent, so a hardware timeout reported by an endpoint goes out as
// INTERNAL_ERROR with the same message.
StatusField status_of(const core::Error& e) {
    StatusField s;
    s.code = (e.code == ErrorCode::NONE || e.code == ErrorCode::TIMEOUT) ? ErrorCode::INTERNAL_ERROR : e.code;
    s.message = wire_text(e.message.empty() ? std::string("operation failed") : e.message);
    return s;
}

StatusField failure(ErrorCode code, const char* message) { return StatusField{code, message}; }

bool same_bytes(ByteSpan a, const std::vector<std::uint8_t>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

} // namespace

core::Result<std::unique_ptr<EdgeHost>> EdgeHost::create(NodeId node, DeviceRegistry& registry, Transport& link) {
    if (!node.valid()) {
        return core::Result<std::unique_ptr<EdgeHost>>::failure(make_error(ErrorCode::INVALID_ARGUMENT, "the Edge node id must be non-zero"));
    }
    return core::Result<std::unique_ptr<EdgeHost>>::success(std::unique_ptr<EdgeHost>(new EdgeHost(node, registry, link)));
}

std::size_t EdgeHost::poll() {
    std::size_t taken = 0;
    while (auto frame = link_.receive()) {
        ++taken;
        handle(*frame);
    }
    return taken;
}

// ---- sending --------------------------------------------------------------------------------------------

void EdgeHost::send(MessageType type, std::uint64_t correlation, std::uint64_t session, std::uint64_t sequence, ByteSpan payload) {
    FrameHeader h;
    h.type = type;
    h.sequence = sequence;
    h.correlation_id = correlation;
    h.session_id = session;
    const auto frame = encode_frame(h, payload);
    if (!frame || !link_.send(frame.value())) {
        ++stats_.send_failures;
    }
}

void EdgeHost::send_response(MessageType type, std::uint64_t correlation, const core::Result<std::vector<std::uint8_t>>& payload) {
    if (!payload) {                                   // an encoder refusal is an internal fault: say so instead of staying silent
        ++stats_.send_failures;
        return;
    }
    send(type, correlation, session_, next_sequence(), payload.value());
}

void EdgeHost::send_protocol_error(const FrameHeader& request, const char* message) {
    ++stats_.protocol_errors_sent;
    const auto payload = encode_protocol_error(StatusPayload{failure(ErrorCode::INVALID_ARGUMENT, message)});
    if (!payload) { ++stats_.send_failures; return; }
    // A PROTOCOL_ERROR that answers a request carries the request's sequence as its correlation id.
    const bool in_session = session_ != 0 && request.session_id == session_;
    send(MessageType::PROTOCOL_ERROR, request.sequence, in_session ? session_ : 0,
         in_session ? next_sequence() : ++unsessioned_sequence_, payload.value());
}

// ---- intake -----------------------------------------------------------------------------------------------

void EdgeHost::handle(const std::vector<std::uint8_t>& bytes) {
    ++stats_.frames_received;
    const DecodedFrame frame = decode_frame(bytes, Receiver::EDGE);
    if (!frame.ok()) {                                // header violations and unknown types: dropped silently, counted
        ++stats_.frame_errors;
        return;
    }
    if (frame.header.type == MessageType::HELLO) {
        handle_hello(frame.header, frame.payload);
        return;
    }
    handle_in_session(frame.header, frame.payload);
}

bool EdgeHost::admit(const FrameHeader& header) {
    if (!passes_stale_check(header.type, header.correlation_id, header.sequence, in_watermark_)) {
        ++stats_.stale_frames;
        return false;
    }
    in_watermark_ = header.sequence;
    last_valid_frame_ns_ = link_.now_ns();
    return true;
}

// ---- HELLO and sessions -----------------------------------------------------------------------------------

void EdgeHost::reject_hello(const FrameHeader& header, ErrorCode code, const char* message) {
    ++stats_.hellos_rejected;
    HelloAckPayload ack;
    ack.status = failure(code, message);
    const auto payload = encode(ack);
    if (!payload) { ++stats_.send_failures; return; }
    // A rejection says nothing about, and changes nothing in, the current session: it goes out unsessioned.
    send(MessageType::HELLO_ACK, header.sequence, 0, ++unsessioned_sequence_, payload.value());
}

void EdgeHost::handle_hello(const FrameHeader& header, ByteSpan payload) {
    if (header.session_id != 0) { reject_hello(header, ErrorCode::INVALID_ARGUMENT, "a HELLO must carry session id 0"); return; }
    if (header.sequence != 1) { reject_hello(header, ErrorCode::INVALID_ARGUMENT, "a HELLO must carry sequence 1"); return; }
    const auto hello = decode<HelloPayload>(payload);
    if (!hello.ok()) {
        ++stats_.hellos_rejected;
        ++stats_.protocol_errors_sent;
        const auto err = encode_protocol_error(StatusPayload{failure(ErrorCode::INVALID_ARGUMENT, "malformed HELLO payload")});
        if (err) send(MessageType::PROTOCOL_ERROR, header.sequence, 0, ++unsessioned_sequence_, err.value());
        else ++stats_.send_failures;
        return;
    }
    const auto version = negotiate(ProtocolVersion{kProtocolMajor, kProtocolMinor}, ProtocolVersion{hello.value.major, hello.value.minor});
    if (!version) { reject_hello(header, ErrorCode::UNSUPPORTED, "unsupported protocol version"); return; }
    LinkTiming timing;
    timing.heartbeat_period_ms = hello.value.heartbeat_period_ms;
    timing.heartbeat_timeout_ms = hello.value.heartbeat_timeout_ms;
    if (!is_valid(timing)) { reject_hello(header, ErrorCode::INVALID_ARGUMENT, "heartbeat timing out of range"); return; }

    // Accepted: from here on the session changes. Order of spec section 9.
    const bool replacing = session_ != 0;
    session_ = 0;                                     // (1) the previous session is invalid from this point
    if (replacing) stop_running_actuators();          // (2)
    ledgers_.clear();                                 // (3) no sequence or write-ledger state is inherited
    in_watermark_ = 0;
    out_sequence_ = 0;
    begin_session();                                  // (4)
    in_watermark_ = header.sequence;                  // the HELLO is the peer's frame 1 of this session
    timing_ = timing;
    last_valid_frame_ns_ = link_.now_ns();
    ++stats_.hellos_accepted;

    HelloAckPayload ack;
    ack.node_id = node_.value();
    ack.major = version->major;
    ack.minor = version->minor;
    ack.session_id = session_;
    ack.heartbeat_period_ms = timing.heartbeat_period_ms;
    ack.heartbeat_timeout_ms = timing.heartbeat_timeout_ms;
    send_response(MessageType::HELLO_ACK, header.sequence, encode(ack));
}

void EdgeHost::begin_session() {
    session_ = ++sessions_issued_;
    if (!sealed_) {                                   // idempotent: the served topology is fixed from the first accepted HELLO
        registry_.close();
        for (Device* d : registry_.devices()) d->seal();
        sealed_ = true;
    }
}

void EdgeHost::stop_running_actuators() {
    for (Device* d : registry_.devices()) {
        for (Endpoint* e : d->endpoints()) {
            if (e->info().direction() != EndpointDirection::ACTUATOR) continue;
            if (e->lifecycle_state() != core::LifecycleState::RUNNING) continue;
            // The existing I3 stop(): an endpoint that fails to stop is FAULT, as I3 defines.
            if (e->stop()) ++stats_.actuators_stopped_on_new_session;
            else ++stats_.actuator_stop_failures;
        }
    }
}

// ---- requests in a session ----------------------------------------------------------------------------------

namespace {

struct Resolved {
    Endpoint* endpoint{nullptr};
    StatusField error;                                // set when endpoint is null
};

} // namespace

void EdgeHost::handle_in_session(const FrameHeader& header, ByteSpan payload) {
    if (session_ == 0) { ++stats_.no_session; return; }
    if (header.session_id != session_) { ++stats_.wrong_session; return; }

    const MessageType type = header.type;
    if (is_response_like(type, header.correlation_id)) { ++stats_.unexpected_frames; return; }   // the Edge has no outstanding requests

    if (type == MessageType::WRITE_REQUEST) { handle_write(header, payload); return; }

    // Decode the payload before the frame is admitted: a malformed frame must not raise the watermark.
    const auto resolve = [&](const AddressPayload& a) -> Resolved {
        if (a.node_id != node_.value()) return {nullptr, failure(ErrorCode::INVALID_ARGUMENT, "unknown node")};
        auto device = registry_.find(DeviceId{a.device_id});
        if (!device) return {nullptr, failure(ErrorCode::INVALID_ARGUMENT, "unknown device")};
        Endpoint* e = device.value()->find_endpoint(EndpointId{a.endpoint_id});
        if (e == nullptr) return {nullptr, failure(ErrorCode::INVALID_ARGUMENT, "unknown endpoint")};
        return {e, {}};
    };
    const auto lifecycle_response = [&](MessageType response, const Resolved& r, core::Result<void> (Endpoint::*op)()) {
        LifecycleResponsePayload out;
        if (r.endpoint == nullptr) {
            out.status = r.error;
        } else if (auto result = (r.endpoint->*op)(); !result) {
            out.status = status_of(result.error());
        } else {
            out.state = r.endpoint->lifecycle_state();
        }
        send_response(response, header.sequence, encode(out));
    };

    switch (type) {
        case MessageType::HEARTBEAT: {
            if (!decode<HeartbeatPayload>(payload).ok()) { ++stats_.frame_errors; return; }   // a notice: nothing to answer
            if (!admit(header)) return;
            ++stats_.heartbeats;
            return;
        }
        case MessageType::PROTOCOL_ERROR: {
            if (!decode_protocol_error(payload).ok()) { ++stats_.frame_errors; return; }
            (void)admit(header);                                                              // counted as peer liveness
            return;
        }
        case MessageType::DISCOVERY_REQUEST: {
            if (decode_empty(payload) != FrameError::NONE) { send_protocol_error(header, "malformed DISCOVERY_REQUEST payload"); return; }
            if (!admit(header)) return;
            ++stats_.requests_served;
            DiscoveryResponsePayload out;
            bool supported = true;
            for (const Device* d : registry_.devices()) {
                DiscoveredDevice dd;
                dd.id = d->info().id().value();
                dd.name = d->info().name();
                for (const Endpoint* e : d->endpoints()) {
                    const auto kind = classify(*e);
                    if (!kind) { supported = false; break; }
                    DiscoveredEndpoint de;
                    de.id = e->info().id().value();
                    de.name = e->info().name();
                    de.direction = e->info().direction() == EndpointDirection::ACTUATOR ? WireDirection::ACTUATOR : WireDirection::SENSOR;
                    de.capabilities.push_back({capability_id_of(*kind), e->capabilities().all().front().name});
                    dd.endpoints.push_back(std::move(de));
                }
                if (!supported) break;
                out.devices.push_back(std::move(dd));
            }
            auto encoded = supported ? encode(out) : core::Result<std::vector<std::uint8_t>>::failure(make_error(ErrorCode::UNSUPPORTED, ""));
            if (!encoded) {                                // not carriable by protocol 1.0: serve no topology
                DiscoveryResponsePayload refusal;
                refusal.status = failure(ErrorCode::UNSUPPORTED, "the topology cannot be served by protocol 1.0");
                encoded = encode(refusal);
            }
            send_response(MessageType::DISCOVERY_RESPONSE, header.sequence, encoded);
            return;
        }
        case MessageType::CONFIGURE_REQUEST: {
            const auto req = decode<ConfigureRequestPayload>(payload);
            if (!req.ok()) { send_protocol_error(header, "malformed CONFIGURE_REQUEST payload"); return; }
            if (!admit(header)) return;
            ++stats_.requests_served;
            LifecycleResponsePayload out;
            const Resolved r = resolve(req.value.address);
            if (r.endpoint == nullptr) {
                out.status = r.error;
            } else {
                // Validate everything before dispatch: known keys only, no duplicates; then the endpoint's own checks.
                const std::vector<std::string> names = r.endpoint->setting_names();
                std::set<std::string> seen;
                core::Configuration scoped;
                for (const ConfigureSetting& s : req.value.settings) {
                    if (std::find(names.begin(), names.end(), s.key) == names.end()) {
                        out.status = failure(ErrorCode::INVALID_ARGUMENT, "unknown setting");
                        break;
                    }
                    if (!seen.insert(s.key).second) { out.status = failure(ErrorCode::INVALID_ARGUMENT, "duplicate setting"); break; }
                    core::ParameterValue value = std::visit([](const auto& v) -> core::ParameterValue { return v; }, s.value);
                    if (!scoped.set(core::Parameter{s.key, std::move(value), {}})) { out.status = failure(ErrorCode::INVALID_ARGUMENT, "invalid setting"); break; }
                }
                if (out.status.ok()) {
                    if (auto result = r.endpoint->configure(scoped); !result) out.status = status_of(result.error());
                    else out.state = r.endpoint->lifecycle_state();
                }
            }
            send_response(MessageType::CONFIGURE_RESPONSE, header.sequence, encode(out));
            return;
        }
        case MessageType::INITIALIZE_REQUEST: case MessageType::START_REQUEST:
        case MessageType::STOP_REQUEST: case MessageType::SHUTDOWN_REQUEST: {
            const auto req = decode<AddressPayload>(payload);
            if (!req.ok()) { send_protocol_error(header, "malformed lifecycle request payload"); return; }
            if (!admit(header)) return;
            ++stats_.requests_served;
            const Resolved r = resolve(req.value);
            switch (type) {
                case MessageType::INITIALIZE_REQUEST: lifecycle_response(MessageType::INITIALIZE_RESPONSE, r, &Endpoint::initialize); break;
                case MessageType::START_REQUEST: lifecycle_response(MessageType::START_RESPONSE, r, &Endpoint::start); break;
                case MessageType::STOP_REQUEST: lifecycle_response(MessageType::STOP_RESPONSE, r, &Endpoint::stop); break;
                default: lifecycle_response(MessageType::SHUTDOWN_RESPONSE, r, &Endpoint::shutdown); break;
            }
            return;
        }
        case MessageType::READ_REQUEST: {
            const auto req = decode<AddressPayload>(payload);
            if (!req.ok()) { send_protocol_error(header, "malformed READ_REQUEST payload"); return; }
            if (!admit(header)) return;
            ++stats_.requests_served;
            ReadResponsePayload out;
            const Resolved r = resolve(req.value);
            if (r.endpoint == nullptr) {
                out.status = r.error;
            } else if (auto reading = read_sensor(*r.endpoint); !reading) {
                out.status = status_of(reading.error());
            } else {
                out.kind = reading.value().kind;
                out.x = reading.value().x;
                out.y = reading.value().y;
                out.z = reading.value().z;
                out.value = reading.value().value;
                out.sample_sequence = reading.value().sample_sequence;
                out.timestamp_ns = reading.value().timestamp_ns;
            }
            send_response(MessageType::READ_RESPONSE, header.sequence, encode(out));
            return;
        }
        case MessageType::OBSERVE_REQUEST: {
            const auto req = decode<AddressPayload>(payload);
            if (!req.ok()) { send_protocol_error(header, "malformed OBSERVE_REQUEST payload"); return; }
            if (!admit(header)) return;
            ++stats_.requests_served;
            ObserveResponsePayload out;
            const Resolved r = resolve(req.value);
            if (r.endpoint == nullptr) {
                out.status = r.error;
            } else {
                const Endpoint& e = *r.endpoint;
                const core::Health health = e.health();
                out.state = e.lifecycle_state();
                out.status_code = e.status().code();
                out.health = health.state();
                out.health_detail = wire_text(health.detail());
                out.operations_ok = e.statistics().sample_count.value();
                out.operations_failed = e.statistics().error_count.value();
                if (e.last_error()) {
                    out.has_last_error = true;
                    out.last_error_code = e.last_error()->code == ErrorCode::NONE || e.last_error()->code == ErrorCode::TIMEOUT
                                              ? ErrorCode::INTERNAL_ERROR : e.last_error()->code;
                    out.last_error_message = wire_text(e.last_error()->message);
                }
            }
            send_response(MessageType::OBSERVE_RESPONSE, header.sequence, encode(out));
            return;
        }
        default:
            ++stats_.unexpected_frames;                // not reachable for the Edge receiver; defensive
            return;
    }
}

// ---- the actuator write path (spec section 12) ----------------------------------------------------------------

void EdgeHost::handle_write(const FrameHeader& header, ByteSpan payload) {
    const auto req = decode<WriteRequestPayload>(payload);
    if (!req.ok()) { send_protocol_error(header, "malformed WRITE_REQUEST payload"); return; }
    const AddressPayload& a = req.value.address;

    // Find the endpoint without answering yet: the duplicate exception below needs its ledger, and a stale frame
    // is dropped before any response.
    Endpoint* endpoint = nullptr;
    if (a.node_id == node_.value()) {
        if (auto device = registry_.find(DeviceId{a.device_id})) endpoint = device.value()->find_endpoint(EndpointId{a.endpoint_id});
    }
    const LedgerKey key{a.device_id, a.endpoint_id};

    // 3. A duplicate of the last applied write (same sequence, identical payload): the cached response again, never applied again.
    if (endpoint != nullptr) {
        const auto it = ledgers_.find(key);
        if (it != ledgers_.end() && header.sequence == it->second.last_applied_sequence && same_bytes(payload, it->second.request_payload)) {
            ++stats_.writes_resent;
            send(MessageType::WRITE_RESPONSE, header.sequence, session_, next_sequence(), it->second.response_payload);
            return;
        }
    }
    // 4. Stale: the request/notice watermark of the session (which is never below the ledger), and the ledger itself.
    const auto ledger = ledgers_.find(key);
    if (ledger != ledgers_.end() && header.sequence <= ledger->second.last_applied_sequence) { ++stats_.stale_frames; return; }
    if (!admit(header)) return;
    ++stats_.requests_served;

    // 2. Address, direction and capability.
    StatusPayload out;
    if (a.node_id != node_.value()) out.status = failure(ErrorCode::INVALID_ARGUMENT, "unknown node");
    else if (!registry_.find(DeviceId{a.device_id})) out.status = failure(ErrorCode::INVALID_ARGUMENT, "unknown device");
    else if (endpoint == nullptr) out.status = failure(ErrorCode::INVALID_ARGUMENT, "unknown endpoint");
    else if (const auto kind = classify(*endpoint); !kind || *kind != EndpointKind::MOTOR_COMMAND) {
        out.status = failure(ErrorCode::INVALID_ARGUMENT, "the endpoint is not a motor command endpoint");
    } else {
        // 5, 6, 7. The I3 endpoint checks its state (NOT_READY, RESOURCE_UNAVAILABLE) and validates the command against
        // its limits before it applies anything; a rejected command changes nothing and is not recorded.
        if (auto result = write_motor_command(*endpoint, req.value.velocity_rad_s); !result) {
            out.status = status_of(result.error());
        } else {
            const auto encoded = encode(out);
            if (encoded) {
                Ledger& l = ledgers_[key];
                l.last_applied_sequence = header.sequence;
                l.request_payload.assign(payload.begin(), payload.end());
                l.response_payload = encoded.value();
            }
            ++stats_.writes_applied;
            send_response(MessageType::WRITE_RESPONSE, header.sequence, encoded);
            return;
        }
    }
    ++stats_.writes_rejected;
    send_response(MessageType::WRITE_RESPONSE, header.sequence, encode(out));
}

} // namespace kritva::hardware::remote
