//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_session.hpp
// Description : The Nexus-side protocol session: HELLO and discovery, request/response exchange with
//               correlation, deadlines and a deterministic pump.
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

#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <optional>
#include <utility>
#include <vector>

#include <kritva/hardware/remote/node.hpp>
#include <kritva/hardware/remote/topology.hpp>
#include <kritva/hardware/transport/codec.hpp>
#include <kritva/hardware/transport/frame.hpp>
#include <kritva/hardware/transport/protocol.hpp>
#include <kritva/hardware/transport/transport.hpp>

namespace kritva::hardware::remote {

/// What the Nexus did with incoming traffic.
struct RemoteStats {
    std::uint64_t requests_sent{0};
    std::uint64_t responses_accepted{0};
    std::uint64_t timeouts{0};
    std::uint64_t late_responses{0};          ///< a response to a request that had already timed out
    std::uint64_t unknown_correlation{0};     ///< a response that matches no outstanding request (unknown, duplicate or already answered)
    std::uint64_t wrong_type{0};              ///< a response of a type that is not the expected one
    std::uint64_t wrong_session{0};           ///< a frame of another session
    std::uint64_t malformed_responses{0};     ///< a matching response whose payload is malformed (the request fails)
    std::uint64_t frame_errors{0};            ///< failed frame validation (spec section 4)
    std::uint64_t notices{0};                 ///< admitted notices (HEARTBEAT, FAULT_EVENT, PROTOCOL_ERROR with correlation 0)
    std::uint64_t stale_notices{0};
    std::uint64_t retransmissions{0};
    std::uint64_t heartbeats_sent{0};
    std::uint64_t heartbeat_send_failures{0};
    std::uint64_t sessions_lost{0};           ///< live sessions that ended (heartbeat timeout, link down, explicit close)
    std::uint64_t degraded_entries{0};
    std::uint64_t malformed_notices{0};
    std::uint64_t fault_events{0};            ///< admitted FAULT_EVENT notices about the expected Edge
    std::uint64_t wrong_node_events{0};
};

/// Why a live session ended. An explicit close is a Nexus-local event: protocol 1.0 has no termination message.
enum class LinkLossKind : std::uint8_t { LINK_LOST, SESSION_CLOSED };

/// The deterministic reason text of a loss, as faulted endpoints and link records carry it.
[[nodiscard]] constexpr const char* reason_of(LinkLossKind kind) noexcept {
    return kind == LinkLossKind::SESSION_CLOSED ? "session closed" : "link lost";
}

/// Handed to the link-lost handler after the session is DISCONNECTED.
struct LinkLoss {
    LinkLossKind kind{LinkLossKind::LINK_LOST};
    std::string reason;                       ///< reason_of(kind): "link lost" (heartbeat timeout or the transport went down) or "session closed"
    std::uint64_t session_id{0};
    std::uint64_t time_ns{0};
};

struct RemoteSessionConfig {
    NodeId nexus_node;                        ///< this node, non-zero
    NodeId edge_node;                         ///< the Edge the session talks to, non-zero
    transport::LinkTiming timing;             ///< heartbeat timing proposed in HELLO, request timeout, pump quantum
    /// Called once per pump step before incoming frames are read, so that a peer in the same process can handle
    /// what has become due (for example `[&] { edge->poll(); }`). The session never knows what it is.
    std::function<void()> peer_tick;
};

/// The Nexus end of one Edge session. Everything is synchronous and deterministic: a call sends one request and
/// pumps the transport's virtual clock in `link.pump_quantum_ms` steps until the matching response or the
/// request deadline (send time + `link.request_timeout_ms`), never longer, with no thread, sleep or wall clock.
///
/// Response admission (protocol section 10) is by session, correlation id, expected response type and
/// outstanding-request state, never by comparing the response's sequence with other frames: a response that
/// arrives after a later heartbeat is accepted. A response is dropped and counted if it matches no
/// outstanding request (unknown, duplicate, already answered, or late after a timeout), has another session id,
/// or has an unexpected type. A matching response whose payload is malformed, and a PROTOCOL_ERROR that matches
/// the request, fail that request at once instead of timing out. Notices (HEARTBEAT, FAULT_EVENT,
/// PROTOCOL_ERROR with correlation 0) use the per-session watermark of the Edge-to-Nexus direction.
///
/// A deadline reached returns the local ErrorCode::TIMEOUT, which is never sent on the wire. The Nexus never
/// retransmits by itself: a timed-out write has an UNKNOWN outcome, and the application may retransmit the
/// SAME request explicitly with retransmit_write(); the Edge's ledger then answers from its cache without
/// applying the command a second time.
///
/// States are the link states of protocol section 9 (not Core lifecycle states). Not here (I4-006):
/// heartbeat transmission, heartbeat timeout, DEGRADED, FAULT_EVENT handling.
class RemoteSession {
public:
    RemoteSession(transport::Transport& link, RemoteSessionConfig config);
    RemoteSession(const RemoteSession&) = delete;
    RemoteSession& operator=(const RemoteSession&) = delete;

    /// Brings the link up if needed, sends HELLO, reads HELLO_ACK, requests discovery and stores the topology:
    /// DISCONNECTED -> CONNECTING -> NEGOTIATING -> CONNECTED. Any failure ends in DISCONNECTED. Not allowed to
    /// replace a topology that is already registered (use reopen()).
    core::Result<void> open();

    /// A fresh session for an initialize: a new HELLO (the Edge replaces its session and stops its running
    /// actuators), a fresh discovery, and a set-based comparison with the topology registered by open(). A
    /// difference fails with CONFIGURATION_ERROR and leaves the session DISCONNECTED.
    core::Result<void> reopen();

    /// Explicit close: the session becomes DISCONNECTED (the handler is told "session closed" if it was live). The
    /// transport is left as it is, and the Edge learns of it only by its own heartbeat timeout: protocol 1.0 has no
    /// termination message, and the Edge's actuator safety is never driven by the Nexus.
    void close();

    using LinkLostHandler = std::function<void(const LinkLoss&)>;
    using FaultEventHandler = std::function<void(const EndpointAddress&, const std::string& reason)>;
    /// The node's reactions: `link_lost` after a LIVE session (CONNECTED or DEGRADED) became DISCONNECTED, once per
    /// loss; `fault_event` for an admitted FAULT_EVENT of the expected Edge.
    void set_handlers(LinkLostHandler link_lost, FaultEventHandler fault_event);

    /// Supervision, against the transport's virtual clock and only when called (the synchronous exchange pump calls it
    /// at every step): a transport that is down, or no liveness for `heartbeat_timeout` (>=), ends the session; no
    /// liveness for 2 heartbeat periods is DEGRADED (observation only, and a valid frame returns to CONNECTED); one
    /// HEARTBEAT is sent if a period has passed (at most one per call). Calling it on a session that is not live does
    /// nothing, so repeated calls after a loss produce no further events.
    void service();

    [[nodiscard]] transport::SessionState state() const noexcept { return state_; }
    /// The current session id, or an invalid id when there is none.
    [[nodiscard]] SessionId session() const noexcept { return SessionId{session_}; }
    [[nodiscard]] NodeId edge_node() const noexcept { return config_.edge_node; }
    [[nodiscard]] const transport::DiscoveryResponsePayload& discovery() const noexcept { return discovery_; }
    [[nodiscard]] const RemoteStats& stats() const noexcept { return stats_; }
    [[nodiscard]] const transport::LinkTiming& timing() const noexcept { return config_.timing; }
    /// The transport's virtual time now (a read, never a drive).
    [[nodiscard]] std::uint64_t now_ns() const noexcept { return link_.now_ns(); }
    /// Virtual time of the last valid in-session frame from the Edge.
    [[nodiscard]] std::uint64_t last_valid_frame_ns() const noexcept { return last_valid_frame_ns_; }
    [[nodiscard]] transport::Transport& link() noexcept { return link_; }
    /// The sequence number the next request of this session will carry (also the correlation id of its response).
    [[nodiscard]] std::uint64_t next_request_sequence() const noexcept { return out_sequence_ + 1; }

    // ---- requests (each needs state CONNECTED, else RESOURCE_UNAVAILABLE without sending) -------------------
    // A failure of the exchange itself (TIMEOUT, link down, malformed response, PROTOCOL_ERROR) is the Result's
    // error; the Edge's own status of a well-formed response is inside the payload.

    core::Result<transport::LifecycleResponsePayload> lifecycle(transport::MessageType request, const EndpointAddress& address);
    core::Result<transport::LifecycleResponsePayload> configure(const EndpointAddress& address, std::vector<transport::ConfigureSetting> settings);
    core::Result<transport::ReadResponsePayload> read(const EndpointAddress& address, transport::ReadKind kind);
    core::Result<transport::StatusPayload> write(const EndpointAddress& address, double velocity_rad_s);
    core::Result<transport::ObserveResponsePayload> observe(const EndpointAddress& address);

    /// Retransmits the SAME last write request of `address` (same sequence, same payload) once, explicitly. INVALID_STATE
    /// if there is none or it belongs to a previous session.
    core::Result<transport::StatusPayload> retransmit_write(const EndpointAddress& address);

    /// The sequence number of the last write request sent to `address` in the current session, if any.
    [[nodiscard]] std::optional<std::uint64_t> last_write_sequence(const EndpointAddress& address) const;

private:
    struct Outstanding {
        transport::MessageType expected{transport::MessageType::HEARTBEAT};
        std::uint64_t deadline_ns{0};
        bool hello{false};
        std::function<bool(transport::ByteSpan)> well_formed;
        bool done{false};
        bool ok{false};
        std::vector<std::uint8_t> payload;
        core::Error error;
    };
    struct SentWrite {
        std::uint64_t session{0};
        std::uint64_t sequence{0};
        std::vector<std::uint8_t> payload;
    };

    core::Result<void> handshake(bool compare);
    core::Result<std::vector<std::uint8_t>> exchange(transport::MessageType type, std::uint64_t sequence, transport::ByteSpan payload, bool hello,
                                                     std::function<bool(transport::ByteSpan)> well_formed);
    core::Result<std::vector<std::uint8_t>> request(transport::MessageType type, transport::ByteSpan payload,
                                                    std::function<bool(transport::ByteSpan)> well_formed);
    void handle(const std::vector<std::uint8_t>& bytes);
    void handle_response(const transport::FrameHeader& header, transport::ByteSpan payload);
    void handle_notice(const transport::FrameHeader& header, transport::ByteSpan payload);
    void to_state(transport::SessionState next) noexcept;
    void end_session(LinkLossKind kind);
    void refresh_liveness();
    void send_heartbeat();
    void disconnect_session() noexcept;
    [[nodiscard]] core::Result<void> require_connected() const;

    transport::Transport& link_;
    RemoteSessionConfig config_;
    transport::SessionState state_{transport::SessionState::DISCONNECTED};
    std::uint64_t session_{0};
    std::uint64_t out_sequence_{0};
    std::uint64_t in_watermark_{0};
    std::uint64_t hello_session_{0};
    std::uint64_t next_heartbeat_ns_{0};
    LinkLostHandler link_lost_;
    FaultEventHandler fault_event_;                         // the session id carried by the HELLO_ACK header
    std::uint64_t last_valid_frame_ns_{0};
    bool registered_{false};
    transport::DiscoveryResponsePayload discovery_;
    Topology registered_topology_;
    std::map<std::uint64_t, Outstanding> outstanding_;       // by correlation id (the request's sequence)
    std::deque<std::uint64_t> timed_out_;                    // bounded memory of timed-out requests, to tell late from unknown
    std::map<std::pair<std::uint64_t, std::uint64_t>, SentWrite> writes_;   // by (device id, endpoint id)
    RemoteStats stats_;
};

} // namespace kritva::hardware::remote
