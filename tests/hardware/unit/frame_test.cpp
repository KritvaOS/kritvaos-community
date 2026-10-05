//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : frame_test.cpp
// Description : Unit tests of frame encoding and strict header validation in the order of spec section 4.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: FR-001; FR-002; FR-003
// API         : FRAME-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <cstring>
#include <vector>

#include "../../runtime/check.hpp"

#include <kritva/hardware/transport/frame.hpp>

using namespace kritva::hardware::transport;
using Bytes = std::vector<std::uint8_t>;

static FrameHeader header(MessageType t = MessageType::READ_REQUEST) {
    FrameHeader h;
    h.type = t;
    h.sequence = 0x0102030405060708ull;
    h.correlation_id = 0x1112131415161718ull;
    h.session_id = 0x2122232425262728ull;
    return h;
}

static Bytes frame_with(MessageType t, std::size_t payload) {
    const Bytes body(payload, 0xAB);
    return encode_frame(header(t), body).value();
}

static void set_le(Bytes& b, std::size_t offset, std::uint64_t v, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) b[offset + i] = static_cast<std::uint8_t>(v >> (8 * i));
}

static void test_exact_bytes_and_little_endian() {                   // FR-001, FR-002
    const Bytes payload{0xDE, 0xAD, 0xBE};
    const auto f = encode_frame(header(), payload);
    KRITVA_CHECK(f.has_value() && f.value().size() == kHeaderSize + 3);
    const Bytes expected{
        0x53, 0x4F, 0x34, 0x4B,                          // magic "K4OS" little-endian
        0x01, 0x00,                                      // major
        0x00, 0x00,                                      // minor
        0x0F, 0x00,                                      // type READ_REQUEST = 0x000F
        0x00, 0x00,                                      // flags
        0x2C, 0x00,                                      // header_length 44
        0x00, 0x00,                                      // reserved
        0x03, 0x00, 0x00, 0x00,                          // payload_length
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,  // sequence
        0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,  // correlation_id
        0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21,  // session_id
        0xDE, 0xAD, 0xBE};
    KRITVA_CHECK(f.value() == expected);                                      // every byte, at the documented offset
}

static void test_round_trip_every_message_type() {
    for (MessageType t : kAllMessageTypes) {
        const Bytes payload(5, 0x5A);
        const auto f = encode_frame(header(t), payload).value();
        const auto d = decode_frame(f);                                       // Receiver::ANY
        KRITVA_CHECK(d.ok() && d.header.type == t && d.header.major == 1 && d.header.minor == 0);
        KRITVA_CHECK(d.header.sequence == 0x0102030405060708ull && d.header.correlation_id == 0x1112131415161718ull &&
                     d.header.session_id == 0x2122232425262728ull);
        KRITVA_CHECK(d.payload.size() == 5 && Bytes(d.payload.begin(), d.payload.end()) == payload);
    }
}

static void test_payload_is_a_bounded_view_inside_the_input() {
    const auto f = frame_with(MessageType::HEARTBEAT, 8);
    const auto d = decode_frame(f);
    KRITVA_CHECK(d.ok() && d.payload.data() == f.data() + kHeaderSize && d.payload.size() == 8);   // no copy, exactly payload_length bytes
    KRITVA_CHECK(d.payload.data() + d.payload.size() == f.data() + f.size());
}

static void test_step1_size() {                                          // TRUNCATED, OVERSIZE
    const auto f = frame_with(MessageType::HEARTBEAT, 0);
    for (std::size_t n = 0; n < kHeaderSize; ++n) {
        KRITVA_CHECK(decode_frame(std::span<const std::uint8_t>(f.data(), n)).error == FrameError::TRUNCATED);   // every prefix of the header
    }
    KRITVA_CHECK(decode_frame(f).ok());
    Bytes big(kMaxFrameSize + 1, 0);
    KRITVA_CHECK(decode_frame(big).error == FrameError::OVERSIZE);
    Bytes huge(kMaxFrameSize + 4096, 0);
    KRITVA_CHECK(decode_frame(huge).error == FrameError::OVERSIZE);
}

static void test_step2_magic() {
    for (std::size_t i = 0; i < 4; ++i) {
        auto f = frame_with(MessageType::HEARTBEAT, 0);
        f[i] ^= 0x01;
        KRITVA_CHECK(decode_frame(f).error == FrameError::BAD_MAGIC);
    }
}

static void test_step3_version() {
    for (auto [major, minor] : {std::pair<std::uint16_t, std::uint16_t>{0, 0}, {2, 0}, {1, 1}, {1, 65535}, {65535, 0}}) {
        auto f = frame_with(MessageType::HEARTBEAT, 0);
        set_le(f, kOffsetMajor, major, 2);
        set_le(f, kOffsetMinor, minor, 2);
        KRITVA_CHECK(decode_frame(f).error == FrameError::UNSUPPORTED_VERSION);
    }
}

static void test_step4_header_fields() {                                  // header_length, flags, reserved
    for (std::uint64_t bad : {std::uint64_t{0}, std::uint64_t{43}, std::uint64_t{45}, std::uint64_t{48}, std::uint64_t{65535}}) {
        auto f = frame_with(MessageType::HEARTBEAT, 0);
        set_le(f, kOffsetHeaderLength, bad, 2);
        KRITVA_CHECK(decode_frame(f).error == FrameError::BAD_HEADER);
    }
    for (std::size_t bit = 0; bit < 16; ++bit) {                          // every single flag bit and every reserved bit
        auto f = frame_with(MessageType::HEARTBEAT, 0);
        set_le(f, kOffsetFlags, 1u << bit, 2);
        KRITVA_CHECK(decode_frame(f).error == FrameError::BAD_HEADER);
        auto g = frame_with(MessageType::HEARTBEAT, 0);
        set_le(g, kOffsetReserved, 1u << bit, 2);
        KRITVA_CHECK(decode_frame(g).error == FrameError::BAD_HEADER);
    }
}

static void test_step5_lengths() {
    auto f = frame_with(MessageType::HEARTBEAT, 8);
    set_le(f, kOffsetPayloadLength, 7, 4);                                // claims less than is there
    KRITVA_CHECK(decode_frame(f).error == FrameError::LENGTH_MISMATCH);
    set_le(f, kOffsetPayloadLength, 9, 4);                                // claims more than is there
    KRITVA_CHECK(decode_frame(f).error == FrameError::LENGTH_MISMATCH);
    set_le(f, kOffsetPayloadLength, 0xFFFFFFFFu, 4);                      // absurd length: rejected before any use
    KRITVA_CHECK(decode_frame(f).error == FrameError::PAYLOAD_TOO_LARGE);
    set_le(f, kOffsetPayloadLength, kMaxPayloadSize + 1, 4);
    KRITVA_CHECK(decode_frame(f).error == FrameError::PAYLOAD_TOO_LARGE);

    // The largest legal frame is accepted exactly; one byte more is not.
    const auto max = frame_with(MessageType::HEARTBEAT, kMaxPayloadSize);
    KRITVA_CHECK(max.size() == kMaxFrameSize && decode_frame(max).ok() && decode_frame(max).payload.size() == kMaxPayloadSize);
    Bytes over = max;
    over.push_back(0);
    KRITVA_CHECK(decode_frame(over).error == FrameError::OVERSIZE);
    auto trailing = frame_with(MessageType::HEARTBEAT, 4);
    trailing.push_back(0);                                                // a trailing byte after the declared payload
    KRITVA_CHECK(decode_frame(trailing).error == FrameError::LENGTH_MISMATCH);
}

static void test_step6_message_type_and_direction() {
    for (std::uint16_t bad : {std::uint16_t{0}, std::uint16_t{0x18}, std::uint16_t{0x100}, std::uint16_t{0xFFFF}}) {
        auto f = frame_with(MessageType::HEARTBEAT, 0);
        set_le(f, kOffsetType, bad, 2);
        KRITVA_CHECK(decode_frame(f).error == FrameError::UNKNOWN_TYPE);
    }
    // Direction is enforced on the receiving side.
    for (MessageType t : kAllMessageTypes) {
        const auto f = frame_with(t, 0);
        const Direction d = direction_of(t);
        KRITVA_CHECK(decode_frame(f, Receiver::ANY).ok());
        KRITVA_CHECK(decode_frame(f, Receiver::EDGE).ok() == (d == Direction::NEXUS_TO_EDGE || d == Direction::BOTH));
        KRITVA_CHECK(decode_frame(f, Receiver::NEXUS).ok() == (d == Direction::EDGE_TO_NEXUS || d == Direction::BOTH));
        if (d == Direction::NEXUS_TO_EDGE) KRITVA_CHECK(decode_frame(f, Receiver::NEXUS).error == FrameError::UNKNOWN_TYPE);
        if (d == Direction::EDGE_TO_NEXUS) KRITVA_CHECK(decode_frame(f, Receiver::EDGE).error == FrameError::UNKNOWN_TYPE);
    }
}

static void test_validation_order() {                                    // the FIRST failing step is reported
    auto f = frame_with(MessageType::HEARTBEAT, 4);
    set_le(f, kOffsetMagic, 0, 4);                                       // bad magic ...
    set_le(f, kOffsetMajor, 9, 2);                                       // ... and bad version ...
    set_le(f, kOffsetFlags, 1, 2);                                       // ... and bad header ...
    set_le(f, kOffsetPayloadLength, 0xFFFFFFFFu, 4);                     // ... and a bad length ...
    set_le(f, kOffsetType, 0, 2);                                        // ... and a bad type
    KRITVA_CHECK(decode_frame(f).error == FrameError::BAD_MAGIC);
    set_le(f, kOffsetMagic, kMagic, 4);
    KRITVA_CHECK(decode_frame(f).error == FrameError::UNSUPPORTED_VERSION);
    set_le(f, kOffsetMajor, 1, 2);
    KRITVA_CHECK(decode_frame(f).error == FrameError::BAD_HEADER);
    set_le(f, kOffsetFlags, 0, 2);
    KRITVA_CHECK(decode_frame(f).error == FrameError::PAYLOAD_TOO_LARGE);
    set_le(f, kOffsetPayloadLength, 4, 4);
    KRITVA_CHECK(decode_frame(f).error == FrameError::UNKNOWN_TYPE);
    set_le(f, kOffsetType, static_cast<std::uint16_t>(MessageType::HEARTBEAT), 2);
    KRITVA_CHECK(decode_frame(f).ok());
}

static void test_codec_boundary_session_rules_are_not_checked() {        // the frame layer judges syntax only
    // session id 0, correlation ids and sequences are session-layer matters: any value decodes.
    FrameHeader h = header(MessageType::READ_REQUEST);
    for (std::uint64_t v : {std::uint64_t{0}, std::uint64_t{1}, ~std::uint64_t{0}}) {
        h.sequence = v; h.correlation_id = v; h.session_id = v;
        const auto d = decode_frame(encode_frame(h, {}).value());
        KRITVA_CHECK(d.ok() && d.header.sequence == v && d.header.correlation_id == v && d.header.session_id == v);
    }
    FrameHeader response = header(MessageType::READ_RESPONSE);            // a response with correlation 0 is a syntactically valid frame
    response.correlation_id = 0;
    KRITVA_CHECK(decode_frame(encode_frame(response, {}).value()).ok());
}

static void test_encode_frame_rejections() {
    const Bytes over(kMaxPayloadSize + 1, 0);
    const auto r = encode_frame(header(), over);
    KRITVA_CHECK(!r.has_value() && r.error().code == kritva::core::ErrorCode::INVALID_ARGUMENT);   // size checked before any output
    FrameHeader bad_type = header();
    bad_type.type = static_cast<MessageType>(0x0042);
    KRITVA_CHECK(!encode_frame(bad_type, {}).has_value());
    FrameHeader bad_version = header();
    bad_version.major = 2;
    KRITVA_CHECK(!encode_frame(bad_version, {}).has_value());
    bad_version.major = 1; bad_version.minor = 1;
    KRITVA_CHECK(!encode_frame(bad_version, {}).has_value());
    KRITVA_CHECK(encode_frame(header(), Bytes(kMaxPayloadSize, 0)).has_value());                   // exactly the limit is fine
}

int main() {
    test_exact_bytes_and_little_endian();
    test_round_trip_every_message_type();
    test_payload_is_a_bounded_view_inside_the_input();
    test_step1_size();
    test_step2_magic();
    test_step3_version();
    test_step4_header_fields();
    test_step5_lengths();
    test_step6_message_type_and_direction();
    test_validation_order();
    test_codec_boundary_session_rules_are_not_checked();
    test_encode_frame_rejections();
    std::printf("frame_test: PASS\n");
    return 0;
}
