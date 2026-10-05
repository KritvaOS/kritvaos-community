//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : codec_fuzz_test.cpp
// Description : Deterministic fixed-seed mutation fuzzing of frames and payloads: never crash, never accept a non-canonical encoding.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: FR-003; PR-002
// API         : CODEC-FUZZ-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <cstdint>
#include <vector>

#include "../../runtime/check.hpp"
#include "../support/codec_samples.hpp"

using namespace kritva::hardware::transport;
using namespace kritva::hardware::transport::test;
using Bytes = std::vector<std::uint8_t>;

// xorshift64*: a fixed seed, so every run is identical; nothing reads a clock or the environment.
struct Rng {
    std::uint64_t state;
    std::uint64_t next() { state ^= state >> 12; state ^= state << 25; state ^= state >> 27; return state * 0x2545F4914F6CDD1Dull; }
    std::uint64_t below(std::uint64_t n) { return n == 0 ? 0 : next() % n; }
};

struct Counters { std::size_t accepted{0}; std::size_t rejected{0}; };

// Decodes `bytes` as the payload of `s` and, when it is accepted, requires the canonical property:
// re-encoding the decoded value gives back exactly the same bytes (so no two byte strings mean the same message).
static void check_payload(const Sample& s, const Bytes& bytes, Counters& c) {
    Bytes again;
    const FrameError e = decode_and_reencode(s.type, s.kind, bytes, again);
    KRITVA_CHECK(e == FrameError::NONE || e == FrameError::BAD_PAYLOAD);          // never any other category, never a contract break
    if (e == FrameError::NONE) {
        ++c.accepted;
        KRITVA_CHECK(again == bytes);
    } else {
        ++c.rejected;
    }
}

static void test_single_bit_flips_of_every_payload() {
    Counters c;
    for (const Sample& s : all_samples()) {
        const std::size_t n = s.payload.size();
        // Every bit of small payloads; for large ones every bit of the first and last 64 bytes plus a stride through the middle.
        for (std::size_t byte = 0; byte < n; ++byte) {
            if (n > 512 && byte >= 64 && byte + 64 < n && byte % 211 != 0) continue;
            for (int bit = 0; bit < 8; ++bit) {
                Bytes m = s.payload;
                m[byte] ^= static_cast<std::uint8_t>(1u << bit);
                check_payload(s, m, c);
            }
        }
    }
    KRITVA_CHECK(c.rejected > 1000 && c.accepted > 100);                       // the flips both break and survive: real coverage, not a no-op
    std::printf("  single bit flips: %zu accepted (canonical), %zu rejected\n", c.accepted, c.rejected);
}

static void test_random_mutations_of_payloads() {
    Rng rng{0x4B344F5301234567ull};
    Counters c;
    for (const Sample& s : all_samples()) {
        for (int round = 0; round < 400; ++round) {
            Bytes m = s.payload;
            const std::uint64_t kind = rng.below(5);
            if (kind == 0 && !m.empty()) {                                      // overwrite 1 to 4 bytes with random values
                for (std::uint64_t k = 0, count = 1 + rng.below(4); k < count; ++k) m[rng.below(m.size())] = static_cast<std::uint8_t>(rng.next());
            } else if (kind == 1 && !m.empty()) {                               // set a byte to an extreme value
                const std::uint8_t extreme[] = {0x00, 0x01, 0x7F, 0x80, 0xFF};
                m[rng.below(m.size())] = extreme[rng.below(5)];
            } else if (kind == 2) {                                             // truncate at a random length
                m.resize(rng.below(m.size() + 1));
            } else if (kind == 3) {                                             // append random bytes
                for (std::uint64_t k = 0, count = 1 + rng.below(9); k < count; ++k) m.push_back(static_cast<std::uint8_t>(rng.next()));
            } else if (!m.empty()) {                                            // splice: duplicate a random slice into a random place
                const std::size_t from = rng.below(m.size()), len = rng.below(m.size() - from + 1);
                const Bytes slice(m.begin() + static_cast<std::ptrdiff_t>(from), m.begin() + static_cast<std::ptrdiff_t>(from + len));
                m.insert(m.begin() + static_cast<std::ptrdiff_t>(rng.below(m.size() + 1)), slice.begin(), slice.end());
            }
            check_payload(s, m, c);
        }
    }
    KRITVA_CHECK(c.rejected > 5000 && c.accepted > 200);
    std::printf("  random mutations: %zu accepted (canonical), %zu rejected\n", c.accepted, c.rejected);
}

static void test_random_garbage_payloads() {
    Rng rng{0x00C0FFEE00FACADEull};
    Counters c;
    const auto samples = all_samples();
    for (int round = 0; round < 20000; ++round) {
        const Sample& s = samples[rng.below(samples.size())];
        Bytes junk(rng.below(120));
        for (auto& b : junk) b = static_cast<std::uint8_t>(rng.next());
        check_payload(s, junk, c);
    }
    std::printf("  random garbage: %zu accepted (canonical), %zu rejected\n", c.accepted, c.rejected);
}

static void test_frame_header_bit_flips_and_garbage() {
    Rng rng{0xDEADBEEF12345678ull};
    Counters c;
    const auto check_frame = [&](const Bytes& bytes) {
        const auto d = decode_frame(bytes);
        if (d.ok()) {
            ++c.accepted;
            // Canonical: re-encoding the decoded header and payload gives the same bytes.
            const auto again = encode_frame(d.header, d.payload);
            KRITVA_CHECK(again.has_value() && again.value() == bytes);
            KRITVA_CHECK(d.payload.data() >= bytes.data() && d.payload.data() + d.payload.size() <= bytes.data() + bytes.size());   // the view stays inside the input
        } else {
            ++c.rejected;
            KRITVA_CHECK(d.error != FrameError::NONE && d.error != FrameError::BAD_PAYLOAD);      // the frame layer never reports a payload error
        }
    };
    for (MessageType t : kAllMessageTypes) {
        FrameHeader h; h.type = t; h.sequence = 9; h.correlation_id = 8; h.session_id = 7;
        const Bytes frame = encode_frame(h, Bytes{1, 2, 3, 4}).value();
        for (std::size_t byte = 0; byte < frame.size(); ++byte) {
            for (int bit = 0; bit < 8; ++bit) { Bytes m = frame; m[byte] ^= static_cast<std::uint8_t>(1u << bit); check_frame(m); }
        }
    }
    for (int round = 0; round < 30000; ++round) {
        Bytes junk(rng.below(200));
        for (auto& b : junk) b = static_cast<std::uint8_t>(rng.next());
        if (junk.size() >= kHeaderSize && rng.below(2) == 0) {                  // make half of the junk start like a real header so deeper checks run
            const Bytes real = encode_frame(FrameHeader{}, Bytes(junk.size() - kHeaderSize, 0)).value();
            std::copy(real.begin(), real.begin() + 16, junk.begin());
        }
        check_frame(junk);
    }
    KRITVA_CHECK(c.rejected > 10000 && c.accepted > 100);
    std::printf("  frames: %zu accepted (canonical), %zu rejected\n", c.accepted, c.rejected);
}

int main() {
    test_single_bit_flips_of_every_payload();
    test_random_mutations_of_payloads();
    test_random_garbage_payloads();
    test_frame_header_bit_flips_and_garbage();
    std::printf("codec_fuzz_test: PASS\n");
    return 0;
}
