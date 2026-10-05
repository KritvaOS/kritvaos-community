//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : codec_truncation_test.cpp
// Description : Exact-consumption, truncation and cross-field tests of every message payload and frame.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: FR-003; PR-002
// API         : CODEC-TRUNCATION-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <vector>

#include "../../runtime/check.hpp"
#include "../support/codec_samples.hpp"

using namespace kritva::hardware::transport;
using namespace kritva::hardware::transport::test;
using kritva::core::ErrorCode;
using Bytes = std::vector<std::uint8_t>;

// Every byte offset is tried for payloads and frames up to 2 KiB (all but the one maximum-size discovery sample); for that one the first and
// last 256 offsets and a stride of 97 through the middle are tried, which keeps the Debug run short without skipping any boundary.
static bool probe(std::size_t n, std::size_t total) { return total <= 2048 || n < 256 || n + 256 >= total || n % 97 == 0; }

static FrameError decode_as(const Sample& s, const Bytes& payload) {
    Bytes again;
    return decode_and_reencode(s.type, s.kind, payload, again);
}

static void test_exact_payload_passes() {
    for (const Sample& s : all_samples()) KRITVA_CHECK(decode_as(s, s.payload) == FrameError::NONE);
}

static void test_every_strict_prefix_is_bad_payload() {                  // valid payload truncated by 1 byte, and by any number of bytes
    for (const Sample& s : all_samples()) {
        for (std::size_t n = 0; n < s.payload.size(); ++n) {
            if (!probe(n, s.payload.size())) continue;
            const Bytes prefix(s.payload.begin(), s.payload.begin() + static_cast<std::ptrdiff_t>(n));
            if (decode_as(s, prefix) != FrameError::BAD_PAYLOAD) {
                std::fprintf(stderr, "sample %s: a prefix of %zu of %zu bytes was not BAD_PAYLOAD\n", s.name.c_str(), n, s.payload.size());
                KRITVA_CHECK(false);
            }
        }
    }
}

static void test_trailing_bytes_are_bad_payload() {                      // protocol 1.0: exact consumption
    for (const Sample& s : all_samples()) {
        for (const Bytes& extra : {Bytes{0x00}, Bytes{0xFF}, Bytes{1, 2, 3, 4, 5, 6, 7}, s.payload}) {
            if (extra.empty()) continue;                                      // (the payload doubled is empty for DISCOVERY_REQUEST)
            Bytes longer = s.payload;
            longer.insert(longer.end(), extra.begin(), extra.end());
            if (decode_as(s, longer) != FrameError::BAD_PAYLOAD) {
                std::fprintf(stderr, "sample %s: %zu trailing bytes were accepted\n", s.name.c_str(), extra.size());
                KRITVA_CHECK(false);
            }
        }
    }
}

static void test_every_frame_prefix_is_rejected() {                       // truncation at every byte offset of every encoded frame
    for (const Sample& s : all_samples()) {
        FrameHeader h;
        h.type = s.type; h.sequence = 5; h.correlation_id = 4; h.session_id = 3;
        const Bytes frame = encode_frame(h, s.payload).value();
        KRITVA_CHECK(decode_frame(frame).ok());
        for (std::size_t n = 0; n < frame.size(); ++n) {
            if (!probe(n, frame.size())) continue;
            const auto d = decode_frame(std::span<const std::uint8_t>(frame.data(), n));
            KRITVA_CHECK(d.error == (n < kHeaderSize ? FrameError::TRUNCATED : FrameError::LENGTH_MISMATCH));
        }
        // A frame that is internally consistent but whose payload is one byte short passes the frame layer and fails the payload decoder.
        if (!s.payload.empty()) {
            const Bytes shorter(s.payload.begin(), s.payload.end() - 1);
            const Bytes short_frame = encode_frame(h, shorter).value();            // the decoded payload is a view into these bytes: keep them alive
            const auto d = decode_frame(short_frame);
            KRITVA_CHECK(d.ok() && decode_as(s, Bytes(d.payload.begin(), d.payload.end())) == FrameError::BAD_PAYLOAD);
        }
    }
}

static void test_cross_field_corruption_and_the_codec_boundary() {
    FrameHeader h;
    h.type = MessageType::WRITE_REQUEST; h.sequence = 10; h.session_id = 7;
    const Bytes payload = encode(WriteRequestPayload{addr(), 0.25}).value();
    const Bytes frame = encode_frame(h, payload).value();

    // Detected by the frame layer: payload_length and header_length against the actual bytes.
    Bytes f = frame; f[kOffsetPayloadLength] ^= 0x01;
    KRITVA_CHECK(decode_frame(f).error == FrameError::LENGTH_MISMATCH);
    f = frame; f[kOffsetHeaderLength] = 0x2D;
    KRITVA_CHECK(decode_frame(f).error == FrameError::BAD_HEADER);

    // Not codec concerns (session layer, I4-003 onward): the frame still decodes and its fields are reported as received.
    f = frame; f[kOffsetCorrelation] = 0x55;                              // a request with a correlation id, or a response with the wrong one
    KRITVA_CHECK(decode_frame(f).ok() && decode_frame(f).header.correlation_id == 0x55);
    f = frame; f[kOffsetSession] ^= 0xFF;                                 // a wrong session id
    KRITVA_CHECK(decode_frame(f).ok());
    f = frame; f[kOffsetSequence] = 0;                                    // a stale sequence
    KRITVA_CHECK(decode_frame(f).ok());
    FrameHeader wrong_type = h;
    wrong_type.type = MessageType::READ_RESPONSE;                         // a response type carrying a request payload: the frame is fine, the payload parse decides
    const Bytes typed_frame = encode_frame(wrong_type, payload).value();
    const auto d = decode_frame(typed_frame);
    KRITVA_CHECK(d.ok());
    KRITVA_CHECK(decode_read_response(d.payload, ReadKind::SCALAR).error == FrameError::BAD_PAYLOAD);   // 32 bytes cannot be a read response

    // Status and message consistency is part of the payload layout, so it IS a codec check.
    KRITVA_CHECK(decode<StatusPayload>(Bytes{0, 0}).ok());                                              // OK, no message
    KRITVA_CHECK(decode<StatusPayload>(Bytes{0, 0, 0, 0}).error == FrameError::BAD_PAYLOAD);            // OK followed by an (empty) message string
    KRITVA_CHECK(decode<StatusPayload>(Bytes{3, 0}).error == FrameError::BAD_PAYLOAD);                  // not OK, message missing
    KRITVA_CHECK(decode<StatusPayload>(Bytes{3, 0, 0, 0}).ok());                                        // not OK with an empty message
    KRITVA_CHECK(decode<StatusPayload>(Bytes{3, 0, 1, 0}).error == FrameError::BAD_PAYLOAD);           // message length 1, byte missing
}

static void test_empty_payload_messages() {
    KRITVA_CHECK(decode_empty({}) == FrameError::NONE);
    const Bytes one{0};
    KRITVA_CHECK(decode_empty(one) == FrameError::BAD_PAYLOAD);
    for (const Sample& s : all_samples()) {
        if (s.type != MessageType::DISCOVERY_REQUEST) KRITVA_CHECK(decode_as(s, {}) == FrameError::BAD_PAYLOAD);   // every other message needs its fields
    }
}

int main() {
    test_exact_payload_passes();
    test_every_strict_prefix_is_bad_payload();
    test_trailing_bytes_are_bad_payload();
    test_every_frame_prefix_is_rejected();
    test_cross_field_corruption_and_the_codec_boundary();
    test_empty_payload_messages();
    std::printf("codec_truncation_test: PASS\n");
    return 0;
}
