//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : frame.hpp
// Description : Protocol frame: header representation, strict header validation and frame encoding.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Transport Codec
// Layer       : Hardware Abstraction
//
// Requirements: FR-001; FR-002; FR-003
// API         : kritva::hardware::transport::decode_frame / encode_frame
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <kritva/core/core.hpp>
#include <kritva/hardware/transport/protocol.hpp>

namespace kritva::hardware::transport {

using ByteSpan = std::span<const std::uint8_t>;

/// The side that receives a frame. Message direction is enforced on the receiving side: a request type
/// arriving at a Nexus, or a response type arriving at an Edge, is UNKNOWN_TYPE for that receiver (spec section 6).
enum class Receiver : std::uint8_t { NEXUS, EDGE, ANY };

/// The decoded fields of a frame header (spec section 4). Fields are plain values: nothing is read from the
/// bytes through a struct overlay.
struct FrameHeader {
    std::uint16_t major{kProtocolMajor};
    std::uint16_t minor{kProtocolMinor};
    MessageType type{MessageType::HEARTBEAT};
    std::uint64_t sequence{0};
    std::uint64_t correlation_id{0};
    std::uint64_t session_id{0};
};

/// The result of validating and splitting an incoming frame. `error` is NONE iff the header passed every check
/// of spec section 4 in order; then `header` is valid and `payload` is the bounded view of exactly
/// `payload_length` bytes inside the input (no copy). Nothing after the first failing step is examined.
struct DecodedFrame {
    FrameError error{FrameError::NONE};
    FrameHeader header;
    ByteSpan payload;
    [[nodiscard]] bool ok() const noexcept { return error == FrameError::NONE; }
};

/// Validates `bytes` as one complete frame in the order of spec section 4:
///   1 TRUNCATED (< header) / OVERSIZE (> MAX_FRAME_SIZE);  2 BAD_MAGIC;  3 UNSUPPORTED_VERSION;
///   4 BAD_HEADER (header_length, flags, reserved);  5 PAYLOAD_TOO_LARGE, LENGTH_MISMATCH;
///   6 UNKNOWN_TYPE (unknown, or wrong direction for `receiver`).
/// It does not parse the payload (BAD_PAYLOAD belongs to the payload decoders) and does not judge session,
/// sequence, correlation or address: those are session-layer rules.
[[nodiscard]] DecodedFrame decode_frame(ByteSpan bytes, Receiver receiver = Receiver::ANY) noexcept;

/// Builds one frame: the 44-byte header written field by field in little-endian order, then `payload`.
/// INVALID_ARGUMENT if the payload exceeds MAX_PAYLOAD_SIZE, the message type is not defined, or the version
/// is not one this implementation can send (major 1, minor <= supported). The size is checked before any
/// byte is produced.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode_frame(const FrameHeader& header, ByteSpan payload);

} // namespace kritva::hardware::transport
