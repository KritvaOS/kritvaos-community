//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : frame_transport_test.cpp
// Description : Integration of the I4-002 frame codec with the I4-003 simulated transport: encoded
//               frames cross the link untouched, and the link takes no protocol decision.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: TR-001; TR-002; FR-001
// API         : FRAME-TRANSPORT-IT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <cstdint>
#include <vector>

#include "../../runtime/check.hpp"
#include "../support/codec_samples.hpp"

#include <kritva/hardware/transport/frame.hpp>
#include <kritva/hardware/transport/simulated_transport.hpp>
#include <kritva/hardware/transport/transport.hpp>

using namespace kritva::hardware::transport;
using Bytes = std::vector<std::uint8_t>;

// The transport owns its own frame bound (it does not include the protocol); the two must agree.
static_assert(kMaxTransportFrameSize == kMaxFrameSize, "the transport frame bound must equal the protocol MAX_FRAME_SIZE");

static Bytes make_frame(MessageType type, std::uint64_t sequence, std::uint64_t correlation, std::uint64_t session, const Bytes& payload) {
    FrameHeader h;
    h.type = type;
    h.sequence = sequence;
    h.correlation_id = correlation;
    h.session_id = session;
    return encode_frame(h, payload).value();
}

// Every valid sample message crosses the link and decodes to the same header and payload.
static void test_every_message_crosses_the_link_unchanged() {
    SimulatedTransport link;
    KRITVA_CHECK(link.connect().has_value());
    DirectionConfig c;
    c.latency_ns = 250;
    KRITVA_CHECK(link.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    std::uint64_t seq = 1;
    for (const auto& s : kritva::hardware::transport::test::all_samples()) {
        const Bytes sent = make_frame(s.type, seq, seq + 100, 7, s.payload);
        KRITVA_CHECK(link.nexus().send(sent).has_value());
        KRITVA_CHECK(!link.edge().receive());                            // not before the due time
        KRITVA_CHECK(link.advance(250).has_value());
        const auto got = link.edge().receive();
        KRITVA_CHECK(got && *got == sent);                               // byte for byte
        const auto d = decode_frame(*got, Receiver::ANY);
        KRITVA_CHECK(d.error == FrameError::NONE && d.header.type == s.type && d.header.sequence == seq &&
                     d.header.session_id == 7 && Bytes(d.payload.begin(), d.payload.end()) == s.payload);
        ++seq;
    }
}

// The link does not interpret frames: garbage, a wrong magic and a stale sequence cross it as readily as
// valid frames. Rejecting them is the codec's and the session layer's job.
static void test_the_link_takes_no_protocol_decision() {
    SimulatedTransport link;
    KRITVA_CHECK(link.connect().has_value());
    Bytes garbage(100, 0xEE);
    const Bytes stale = make_frame(MessageType::HEARTBEAT, 1, 0, 9, Bytes(8, 0));       // sequence 1 after a later one
    const Bytes later = make_frame(MessageType::HEARTBEAT, 5, 0, 9, Bytes(8, 0));
    Bytes bad_magic = later;
    bad_magic[0] ^= 0xFF;
    KRITVA_CHECK(link.nexus().send(later).has_value() && link.nexus().send(stale).has_value() &&
                 link.nexus().send(garbage).has_value() && link.nexus().send(bad_magic).has_value());
    KRITVA_CHECK(*link.edge().receive() == later);
    KRITVA_CHECK(*link.edge().receive() == stale);                       // no stale-sequence filtering in the link
    KRITVA_CHECK(*link.edge().receive() == garbage);
    const auto last = link.edge().receive();
    KRITVA_CHECK(last && *last == bad_magic);
    KRITVA_CHECK(decode_frame(*last, Receiver::ANY).error == FrameError::BAD_MAGIC);    // the codec, not the link, rejects it
    KRITVA_CHECK(link.stats(LinkDirection::NEXUS_TO_EDGE).dropped == 0 && link.stats(LinkDirection::NEXUS_TO_EDGE).delivered == 4);
}

// A duplicate or reordered frame arrives exactly as sent, so the layers above see the same bytes twice
// or in another order and must apply the I4-001 admission rules themselves.
static void test_duplicate_and_reorder_deliver_identical_bytes() {
    SimulatedTransport link;
    KRITVA_CHECK(link.connect().has_value());
    DirectionConfig c;
    c.latency_ns = 10;
    c.faults[0].duplicate = true;
    c.faults[0].duplicate_extra_delay_ns = 5;
    c.faults[1].extra_delay_ns = 100;
    KRITVA_CHECK(link.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    const Bytes w1 = make_frame(MessageType::WRITE_REQUEST, 1, 11, 3, Bytes{1});
    const Bytes w2 = make_frame(MessageType::WRITE_REQUEST, 2, 12, 3, Bytes{2});
    const Bytes w3 = make_frame(MessageType::WRITE_REQUEST, 3, 13, 3, Bytes{3});
    for (const Bytes* f : {&w1, &w2, &w3}) KRITVA_CHECK(link.nexus().send(*f).has_value());
    std::vector<Bytes> seen;
    while (link.step()) while (auto f = link.edge().receive()) seen.push_back(*f);
    KRITVA_CHECK((seen == std::vector<Bytes>{w1, w3, w1, w2}));          // w1, w3 on time; the copy of w1; w2 held back past w3
}

int main() {
    test_every_message_crosses_the_link_unchanged();
    test_the_link_takes_no_protocol_decision();
    test_duplicate_and_reorder_deliver_identical_bytes();
    std::printf("frame_transport_test: PASS\n");
    return 0;
}
