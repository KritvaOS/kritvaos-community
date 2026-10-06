//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : edge_host.hpp
// Description : The Edge-side service: serves sealed I3 Devices and Endpoints to a Nexus over a Transport.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: ER-001; ER-002; ER-003; SR-001; SR-002 (mechanism; end-to-end verification in I4-006)
// API         : kritva::hardware::remote::EdgeHost
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <kritva/hardware/device_registry.hpp>
#include <kritva/hardware/remote/node.hpp>
#include <kritva/hardware/transport/codec.hpp>
#include <kritva/hardware/transport/frame.hpp>
#include <kritva/hardware/transport/protocol.hpp>
#include <kritva/hardware/transport/transport.hpp>

namespace kritva::hardware::remote {

/// Counters of what the Edge did with incoming traffic (nothing here is a safety decision).
struct EdgeStats {
    std::uint64_t frames_received{0};
    std::uint64_t frame_errors{0};            ///< failed frame validation (spec section 4); dropped silently
    std::uint64_t no_session{0};              ///< a non-HELLO frame before any session
    std::uint64_t wrong_session{0};           ///< a frame of another session; dropped
    std::uint64_t stale_frames{0};            ///< request or notice with sequence <= the watermark; dropped
    std::uint64_t unexpected_frames{0};       ///< a response-like frame at the Edge; dropped
    std::uint64_t protocol_errors_sent{0};    ///< valid-header known request with a malformed payload
    std::uint64_t hellos_accepted{0};
    std::uint64_t hellos_rejected{0};
    std::uint64_t requests_served{0};         ///< admitted requests answered (success or error status)
    std::uint64_t heartbeats{0};              ///< valid heartbeats received
    std::uint64_t writes_applied{0};
    std::uint64_t writes_rejected{0};         ///< admitted writes that did not apply (address, state, limits)
    std::uint64_t writes_resent{0};           ///< duplicate of the last applied write: cached response sent again, not applied
    std::uint64_t send_failures{0};           ///< a frame the transport refused
    std::uint64_t actuators_stopped_on_new_session{0};
    std::uint64_t actuator_stop_failures{0};
};

/// The Edge service of KOS-I4 (docs/architecture/KOS-I4_PROTOCOL.md). It is an I4 service, not a Core
/// Component, and it adds no second RuntimeHost, RuntimeManager or DeviceManager: it serves Devices that the
/// integrator owns and registered in a plain I3 DeviceRegistry, and it executes the Nexus lifecycle, data and
/// diagnostic requests on their existing I3 Endpoints, returning each endpoint's own error unchanged.
///
/// Driving: poll() handles every frame the transport has made receivable and answers through the same
/// transport. There are no threads, no sleeping and no clock of its own: time is the transport's.
///
/// What this class decides (and the Transport does not): session validity, the per-session request/notice
/// sequence watermark, address validation, the typed capability mapping, the write ledger and the Edge-side
/// safety order of spec section 12. An actuator write reaches hardware only through
/// ActuatorEndpoint<MotorCommand>::write (see capability_dispatch.hpp), so the I3 state check and limit
/// validation run before anything is applied.
///
/// Sessions (spec section 9). Only an ACCEPTED HELLO changes session state: a rejected HELLO (malformed,
/// unsupported version, invalid timing, wrong header fields) has no observable effect on the current session.
/// An accepted HELLO ALWAYS starts a new session and invalidates the previous one; protocol 1.0 has no HELLO
/// deduplication, so a duplicated HELLO replaces the session it just created and peers must not treat HELLO
/// as idempotent. Replacing a session is an Edge-side safety action, in this order: (1) the previous
/// SessionId becomes invalid; (2) every actuator endpoint that is RUNNING is stopped through
/// Endpoint::stop(); (3) the sequence watermark and every write ledger are reset; (4) the new SessionId is
/// established and the served Devices are sealed (idempotent). The new session inherits no sequence or ledger
/// state.
///
/// Not here (I4-006): heartbeat transmission and timeout supervision, stopping actuators on a heartbeat
/// timeout, FAULT_EVENT, DEGRADED. A valid heartbeat is only recorded (last_valid_frame_ns()).
///
/// Lifetimes: the registry, its Devices and the transport must outlive the EdgeHost. Single-threaded.
class EdgeHost {
public:
    /// `node` must be valid (non-zero). The EdgeHost keeps references to `registry` and `link` (the Edge end).
    [[nodiscard]] static core::Result<std::unique_ptr<EdgeHost>> create(NodeId node, DeviceRegistry& registry, transport::Transport& link);

    EdgeHost(const EdgeHost&) = delete;
    EdgeHost& operator=(const EdgeHost&) = delete;

    /// Handles every frame that is receivable now and returns how many it took from the transport.
    std::size_t poll();

    [[nodiscard]] NodeId node() const noexcept { return node_; }
    /// The current session, or an invalid id when there is none.
    [[nodiscard]] SessionId session() const noexcept { return SessionId{session_}; }
    [[nodiscard]] bool sealed() const noexcept { return sealed_; }
    /// The heartbeat timing accepted in the current HELLO (for the I4-006 supervision); zeros without a session.
    [[nodiscard]] const transport::LinkTiming& timing() const noexcept { return timing_; }
    /// Virtual time of the last frame accepted in the current session (any valid in-session frame).
    [[nodiscard]] std::uint64_t last_valid_frame_ns() const noexcept { return last_valid_frame_ns_; }
    [[nodiscard]] const EdgeStats& stats() const noexcept { return stats_; }

private:
    struct Ledger {
        std::uint64_t last_applied_sequence{0};
        std::vector<std::uint8_t> request_payload;     // the applied WRITE_REQUEST payload, for the duplicate test
        std::vector<std::uint8_t> response_payload;    // the cached WRITE_RESPONSE payload
    };
    using LedgerKey = std::pair<std::uint64_t, std::uint64_t>;   // (device id, endpoint id)

    EdgeHost(NodeId node, DeviceRegistry& registry, transport::Transport& link) noexcept
        : node_(node), registry_(registry), link_(link) {}

    void handle(const std::vector<std::uint8_t>& bytes);
    void handle_hello(const transport::FrameHeader& header, transport::ByteSpan payload);
    void handle_in_session(const transport::FrameHeader& header, transport::ByteSpan payload);
    void handle_write(const transport::FrameHeader& header, transport::ByteSpan payload);
    void begin_session();
    void stop_running_actuators();
    [[nodiscard]] bool admit(const transport::FrameHeader& header);   // watermark: raises it on success

    void reject_hello(const transport::FrameHeader& header, core::ErrorCode code, const char* message);
    void send_response(transport::MessageType type, std::uint64_t correlation, const core::Result<std::vector<std::uint8_t>>& payload);
    void send_protocol_error(const transport::FrameHeader& request, const char* message);
    void send(transport::MessageType type, std::uint64_t correlation, std::uint64_t session, std::uint64_t sequence,
              transport::ByteSpan payload);
    [[nodiscard]] std::uint64_t next_sequence() noexcept { return ++out_sequence_; }

    NodeId node_;
    DeviceRegistry& registry_;
    transport::Transport& link_;
    bool sealed_{false};
    std::uint64_t session_{0};               // 0: no session
    std::uint64_t sessions_issued_{0};
    std::uint64_t in_watermark_{0};          // highest accepted request/notice sequence of this session
    std::uint64_t out_sequence_{0};          // this side's frame counter in this session
    std::uint64_t unsessioned_sequence_{0};  // counter for frames sent outside a session (rejected HELLO)
    std::uint64_t last_valid_frame_ns_{0};
    transport::LinkTiming timing_{0, 0, 0, 0};
    std::map<LedgerKey, Ledger> ledgers_;
    EdgeStats stats_;
};

} // namespace kritva::hardware::remote
