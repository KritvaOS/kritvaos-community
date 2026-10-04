//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : protocol.hpp
// Description : Nexus-Edge protocol contract: constants, message types, version rules, status mapping, session states.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Transport Protocol
// Layer       : Hardware Abstraction
//
// Requirements: FR-001; FR-002; PR-001; PR-002; PR-003; NDR-003; TR-003
// API         : kritva::hardware::transport (see docs/architecture/KOS-I4_PROTOCOL.md)
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <kritva/core/core.hpp>

// The normative text is docs/architecture/KOS-I4_PROTOCOL.md; every constant and table here is
// checked against it by the test `kritva_protocol_spec_consistency`.
namespace kritva::hardware::transport {

// ---- constants (spec section 3) ------------------------------------------------------------

inline constexpr std::uint32_t kMagic = 0x4B344F53;                 // "K4OS"; wire bytes 53 4F 34 4B
inline constexpr std::uint16_t kProtocolMajor = 1;
inline constexpr std::uint16_t kProtocolMinor = 0;
inline constexpr std::size_t kHeaderSize = 44;
inline constexpr std::size_t kMaxFrameSize = 65536;
inline constexpr std::size_t kMaxPayloadSize = kMaxFrameSize - kHeaderSize;
inline constexpr std::size_t kMaxNameLength = 64;
inline constexpr std::size_t kMaxTextLength = 256;
inline constexpr std::size_t kMaxDevices = 64;
inline constexpr std::size_t kMaxDiscoveryItems = 256;
inline constexpr std::size_t kMaxCapabilitiesPerEndpoint = 16;
inline constexpr std::size_t kMaxConfigureSettings = 16;

// ---- frame header offsets (spec section 4) -------------------------------------------------

inline constexpr std::size_t kOffsetMagic = 0;
inline constexpr std::size_t kOffsetMajor = 4;
inline constexpr std::size_t kOffsetMinor = 6;
inline constexpr std::size_t kOffsetType = 8;
inline constexpr std::size_t kOffsetFlags = 10;
inline constexpr std::size_t kOffsetHeaderLength = 12;
inline constexpr std::size_t kOffsetReserved = 14;
inline constexpr std::size_t kOffsetPayloadLength = 16;
inline constexpr std::size_t kOffsetSequence = 20;
inline constexpr std::size_t kOffsetCorrelation = 28;
inline constexpr std::size_t kOffsetSession = 36;
static_assert(kOffsetSession + 8 == kHeaderSize, "the header layout must add up to HEADER_SIZE");

// ---- message types (spec section 6) --------------------------------------------------------

enum class MessageType : std::uint16_t {
    HELLO = 0x0001,               HELLO_ACK = 0x0002,
    DISCOVERY_REQUEST = 0x0003,   DISCOVERY_RESPONSE = 0x0004,
    CONFIGURE_REQUEST = 0x0005,   CONFIGURE_RESPONSE = 0x0006,
    INITIALIZE_REQUEST = 0x0007,  INITIALIZE_RESPONSE = 0x0008,
    START_REQUEST = 0x0009,       START_RESPONSE = 0x000A,
    STOP_REQUEST = 0x000B,        STOP_RESPONSE = 0x000C,
    SHUTDOWN_REQUEST = 0x000D,    SHUTDOWN_RESPONSE = 0x000E,
    READ_REQUEST = 0x000F,        READ_RESPONSE = 0x0010,
    WRITE_REQUEST = 0x0011,       WRITE_RESPONSE = 0x0012,
    OBSERVE_REQUEST = 0x0013,     OBSERVE_RESPONSE = 0x0014,
    FAULT_EVENT = 0x0015,
    HEARTBEAT = 0x0016,
    PROTOCOL_ERROR = 0x0017,
};

/// Every defined message type, in numeric order.
inline constexpr std::array<MessageType, 23> kAllMessageTypes{
    MessageType::HELLO, MessageType::HELLO_ACK, MessageType::DISCOVERY_REQUEST, MessageType::DISCOVERY_RESPONSE,
    MessageType::CONFIGURE_REQUEST, MessageType::CONFIGURE_RESPONSE, MessageType::INITIALIZE_REQUEST,
    MessageType::INITIALIZE_RESPONSE, MessageType::START_REQUEST, MessageType::START_RESPONSE,
    MessageType::STOP_REQUEST, MessageType::STOP_RESPONSE, MessageType::SHUTDOWN_REQUEST,
    MessageType::SHUTDOWN_RESPONSE, MessageType::READ_REQUEST, MessageType::READ_RESPONSE,
    MessageType::WRITE_REQUEST, MessageType::WRITE_RESPONSE, MessageType::OBSERVE_REQUEST,
    MessageType::OBSERVE_RESPONSE, MessageType::FAULT_EVENT, MessageType::HEARTBEAT, MessageType::PROTOCOL_ERROR};

[[nodiscard]] constexpr std::optional<MessageType> message_type_from_wire(std::uint16_t value) noexcept {
    return value >= 0x0001 && value <= 0x0017 ? std::optional<MessageType>(static_cast<MessageType>(value)) : std::nullopt;
}

/// The name used in the specification table (for example "READ_REQUEST").
[[nodiscard]] const char* message_type_name(MessageType type) noexcept;

enum class Direction : std::uint8_t { NEXUS_TO_EDGE, EDGE_TO_NEXUS, BOTH };

[[nodiscard]] constexpr Direction direction_of(MessageType t) noexcept {
    switch (t) {
        case MessageType::HELLO: case MessageType::DISCOVERY_REQUEST: case MessageType::CONFIGURE_REQUEST:
        case MessageType::INITIALIZE_REQUEST: case MessageType::START_REQUEST: case MessageType::STOP_REQUEST:
        case MessageType::SHUTDOWN_REQUEST: case MessageType::READ_REQUEST: case MessageType::WRITE_REQUEST:
        case MessageType::OBSERVE_REQUEST:
            return Direction::NEXUS_TO_EDGE;
        case MessageType::HEARTBEAT: case MessageType::PROTOCOL_ERROR:
            return Direction::BOTH;
        default:
            return Direction::EDGE_TO_NEXUS;
    }
}

/// A request is a message answered by exactly one response (the Nexus-to-Edge types except the notices).
[[nodiscard]] constexpr bool is_request(MessageType t) noexcept {
    return direction_of(t) == Direction::NEXUS_TO_EDGE;
}

/// The response type of a request, or nullopt for a type that is not a request.
[[nodiscard]] constexpr std::optional<MessageType> response_for(MessageType request) noexcept {
    return is_request(request) ? std::optional<MessageType>(static_cast<MessageType>(static_cast<std::uint16_t>(request) + 1))
                               : std::nullopt;
}

/// The messages that have no response: FAULT_EVENT, HEARTBEAT and PROTOCOL_ERROR.
[[nodiscard]] constexpr bool is_notice(MessageType t) noexcept {
    return t == MessageType::FAULT_EVENT || t == MessageType::HEARTBEAT || t == MessageType::PROTOCOL_ERROR;
}

// ---- version negotiation (spec sections 4, 9, 16) --------------------------------------------

struct ProtocolVersion {
    std::uint16_t major{kProtocolMajor};
    std::uint16_t minor{kProtocolMinor};
    friend constexpr bool operator==(ProtocolVersion, ProtocolVersion) noexcept = default;
};

/// A receiver supporting `local` accepts a peer proposing `peer` iff the major numbers are equal
/// and `peer.minor <= local.minor`; the negotiated version is the peer's. Otherwise nullopt.
[[nodiscard]] constexpr std::optional<ProtocolVersion> negotiate(ProtocolVersion local, ProtocolVersion peer) noexcept {
    return peer.major == local.major && peer.minor <= local.minor ? std::optional<ProtocolVersion>(peer) : std::nullopt;
}

// ---- status mapping (spec section 8) ----------------------------------------------------------

/// The wire status is the numeric value of the released Core ErrorCode; 0 is OK.
inline constexpr std::uint16_t kMaxWireStatus = 11;

[[nodiscard]] constexpr std::uint16_t to_wire_status(core::ErrorCode code) noexcept {
    return static_cast<std::uint16_t>(static_cast<std::uint32_t>(code));
}

/// nullopt for a value above kMaxWireStatus (the payload is then malformed).
[[nodiscard]] constexpr std::optional<core::ErrorCode> from_wire_status(std::uint16_t value) noexcept {
    return value <= kMaxWireStatus ? std::optional<core::ErrorCode>(static_cast<core::ErrorCode>(value)) : std::nullopt;
}

// ---- frame validation outcomes (spec section 4, validation order; section 15) -------------------

enum class FrameError : std::uint8_t {
    NONE, TRUNCATED, OVERSIZE, BAD_MAGIC, UNSUPPORTED_VERSION, BAD_HEADER, PAYLOAD_TOO_LARGE, LENGTH_MISMATCH,
    UNKNOWN_TYPE, BAD_PAYLOAD,
};

// ---- session states (spec section 9) -----------------------------------------------------------

/// A Nexus-side link state. NOT a Core LifecycleState and never mapped onto one.
enum class SessionState : std::uint8_t { DISCONNECTED, CONNECTING, NEGOTIATING, CONNECTED, DEGRADED };

[[nodiscard]] const char* session_state_name(SessionState state) noexcept;

/// The transition table of spec section 9. Every pair not listed is invalid.
[[nodiscard]] constexpr bool is_valid_transition(SessionState from, SessionState to) noexcept {
    using S = SessionState;
    switch (from) {
        case S::DISCONNECTED: return to == S::CONNECTING;
        case S::CONNECTING:   return to == S::NEGOTIATING || to == S::DISCONNECTED;
        case S::NEGOTIATING:  return to == S::CONNECTED || to == S::DISCONNECTED;
        case S::CONNECTED:    return to == S::DEGRADED || to == S::DISCONNECTED;
        case S::DEGRADED:     return to == S::CONNECTED || to == S::DISCONNECTED;
    }
    return false;
}

// ---- link timing (spec section 14) --------------------------------------------------------------

inline constexpr const char* kKeyHeartbeatPeriodMs = "link.heartbeat_period_ms";
inline constexpr const char* kKeyHeartbeatTimeoutMs = "link.heartbeat_timeout_ms";
inline constexpr const char* kKeyRequestTimeoutMs = "link.request_timeout_ms";
inline constexpr const char* kKeyPumpQuantumMs = "link.pump_quantum_ms";

struct LinkTiming {
    std::uint32_t heartbeat_period_ms{100};
    std::uint32_t heartbeat_timeout_ms{300};
    std::uint32_t request_timeout_ms{100};
    std::uint32_t pump_quantum_ms{1};
};

/// True iff every value is inside its range and the cross-constraints hold (timeout >= 2 x period,
/// quantum <= request timeout); see the table in spec section 14.
[[nodiscard]] constexpr bool is_valid(const LinkTiming& t) noexcept {
    return t.heartbeat_period_ms >= 10 && t.heartbeat_period_ms <= 60000 &&
           t.heartbeat_timeout_ms >= 20 && t.heartbeat_timeout_ms <= 600000 &&
           t.heartbeat_timeout_ms >= 2ull * t.heartbeat_period_ms &&
           t.request_timeout_ms >= 1 && t.request_timeout_ms <= 60000 &&
           t.pump_quantum_ms >= 1 && t.pump_quantum_ms <= 1000 && t.pump_quantum_ms <= t.request_timeout_ms;
}

} // namespace kritva::hardware::transport
