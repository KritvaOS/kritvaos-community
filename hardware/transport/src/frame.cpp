//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : frame.cpp
// Description : Frame header validation and encoding.
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

#include <kritva/hardware/transport/frame.hpp>

namespace kritva::hardware::transport {

namespace {

// Field readers over the already length-checked header bytes (little-endian, one byte at a time).
std::uint64_t le(ByteSpan b, std::size_t offset, std::size_t size) noexcept {
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < size; ++i) v |= static_cast<std::uint64_t>(b[offset + i]) << (8 * i);
    return v;
}

void put(std::vector<std::uint8_t>& out, std::uint64_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

bool direction_accepts(MessageType type, Receiver receiver) noexcept {
    if (receiver == Receiver::ANY) return true;
    const Direction d = direction_of(type);
    if (d == Direction::BOTH) return true;
    return receiver == Receiver::EDGE ? d == Direction::NEXUS_TO_EDGE : d == Direction::EDGE_TO_NEXUS;
}

} // namespace

DecodedFrame decode_frame(ByteSpan bytes, Receiver receiver) noexcept {
    DecodedFrame out;
    const auto fail = [&](FrameError e) { out.error = e; return out; };

    // 1. size
    if (bytes.size() < kHeaderSize) return fail(FrameError::TRUNCATED);
    if (bytes.size() > kMaxFrameSize) return fail(FrameError::OVERSIZE);
    // 2. magic
    if (le(bytes, kOffsetMagic, 4) != kMagic) return fail(FrameError::BAD_MAGIC);
    // 3. version
    const auto major = static_cast<std::uint16_t>(le(bytes, kOffsetMajor, 2));
    const auto minor = static_cast<std::uint16_t>(le(bytes, kOffsetMinor, 2));
    if (major != kProtocolMajor || minor > kProtocolMinor) return fail(FrameError::UNSUPPORTED_VERSION);
    // 4. header_length, flags, reserved
    if (le(bytes, kOffsetHeaderLength, 2) != kHeaderSize || le(bytes, kOffsetFlags, 2) != 0 || le(bytes, kOffsetReserved, 2) != 0) {
        return fail(FrameError::BAD_HEADER);
    }
    // 5. lengths
    const std::uint64_t payload_length = le(bytes, kOffsetPayloadLength, 4);
    if (payload_length > kMaxPayloadSize) return fail(FrameError::PAYLOAD_TOO_LARGE);
    if (bytes.size() != kHeaderSize + payload_length) return fail(FrameError::LENGTH_MISMATCH);
    // 6. message type (and direction for this receiver)
    const auto type = message_type_from_wire(static_cast<std::uint16_t>(le(bytes, kOffsetType, 2)));
    if (!type.has_value() || !direction_accepts(*type, receiver)) return fail(FrameError::UNKNOWN_TYPE);

    out.header = FrameHeader{major, minor, *type, le(bytes, kOffsetSequence, 8), le(bytes, kOffsetCorrelation, 8), le(bytes, kOffsetSession, 8)};
    out.payload = bytes.subspan(kHeaderSize, static_cast<std::size_t>(payload_length));   // bounded: exactly payload_length bytes
    return out;
}

core::Result<std::vector<std::uint8_t>> encode_frame(const FrameHeader& header, ByteSpan payload) {
    using R = core::Result<std::vector<std::uint8_t>>;
    const auto invalid = [](const char* why) {
        return R::failure(core::Error{core::ErrorCode::INVALID_ARGUMENT, core::ErrorSeverity::ERROR, {}, {}, why});
    };
    if (payload.size() > kMaxPayloadSize) return invalid("payload exceeds MAX_PAYLOAD_SIZE");          // size first, before any output
    if (!message_type_from_wire(static_cast<std::uint16_t>(header.type)).has_value()) return invalid("undefined message type");
    if (header.major != kProtocolMajor || header.minor > kProtocolMinor) return invalid("unsupported protocol version");

    std::vector<std::uint8_t> frame;
    frame.reserve(kHeaderSize + payload.size());
    put(frame, kMagic, 4);
    put(frame, header.major, 2);
    put(frame, header.minor, 2);
    put(frame, static_cast<std::uint16_t>(header.type), 2);
    put(frame, 0, 2);                                   // flags
    put(frame, kHeaderSize, 2);                         // header_length
    put(frame, 0, 2);                                   // reserved
    put(frame, payload.size(), 4);
    put(frame, header.sequence, 8);
    put(frame, header.correlation_id, 8);
    put(frame, header.session_id, 8);
    frame.insert(frame.end(), payload.begin(), payload.end());
    return R::success(std::move(frame));
}

} // namespace kritva::hardware::transport
