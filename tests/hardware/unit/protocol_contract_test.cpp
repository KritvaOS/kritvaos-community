//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : protocol_contract_test.cpp
// Description : Unit tests of the Nexus-Edge protocol contract constants, tables and rules.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: FR-001; FR-002; PR-001..003; NDR-003; TR-003
// API         : PROTOCOL-CONTRACT-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <set>
#include <string>
#include <type_traits>

#include "../../runtime/check.hpp"

#include <kritva/hardware/transport/protocol.hpp>

using namespace kritva::hardware::transport;
using kritva::core::ErrorCode;

// ---- compile-time facts -------------------------------------------------------------------------
static_assert(kHeaderSize == 44 && kMaxFrameSize == 65536 && kMaxPayloadSize == 65536 - 44);
static_assert(kMagic == 0x4B344F53 && kProtocolMajor == 1 && kProtocolMinor == 0);
static_assert(kOffsetMagic == 0 && kOffsetMajor == 4 && kOffsetMinor == 6 && kOffsetType == 8 && kOffsetFlags == 10 &&
              kOffsetHeaderLength == 12 && kOffsetReserved == 14 && kOffsetPayloadLength == 16 && kOffsetSequence == 20 &&
              kOffsetCorrelation == 28 && kOffsetSession == 36);
// The status mapping is the numeric value of the released Core ErrorCode: guard against Core drift.
static_assert(static_cast<unsigned>(ErrorCode::NONE) == 0 && static_cast<unsigned>(ErrorCode::UNKNOWN) == 1 &&
              static_cast<unsigned>(ErrorCode::INVALID_ARGUMENT) == 2 && static_cast<unsigned>(ErrorCode::INVALID_STATE) == 3 &&
              static_cast<unsigned>(ErrorCode::NOT_INITIALIZED) == 4 && static_cast<unsigned>(ErrorCode::NOT_READY) == 5 &&
              static_cast<unsigned>(ErrorCode::ALREADY_RUNNING) == 6 && static_cast<unsigned>(ErrorCode::TIMEOUT) == 7 &&
              static_cast<unsigned>(ErrorCode::RESOURCE_UNAVAILABLE) == 8 && static_cast<unsigned>(ErrorCode::CONFIGURATION_ERROR) == 9 &&
              static_cast<unsigned>(ErrorCode::UNSUPPORTED) == 10 && static_cast<unsigned>(ErrorCode::INTERNAL_ERROR) == 11);
// The session state is its own type: it cannot be confused with the Core lifecycle.
static_assert(!std::is_same_v<SessionState, kritva::core::LifecycleState>);
static_assert(!std::is_convertible_v<SessionState, kritva::core::LifecycleState>);

static void test_magic_on_the_wire() {
    const std::uint8_t wire[4] = {static_cast<std::uint8_t>(kMagic), static_cast<std::uint8_t>(kMagic >> 8),
                                  static_cast<std::uint8_t>(kMagic >> 16), static_cast<std::uint8_t>(kMagic >> 24)};
    KRITVA_CHECK(wire[0] == 0x53 && wire[1] == 0x4F && wire[2] == 0x34 && wire[3] == 0x4B);   // little-endian bytes 'S','O','4','K'
    KRITVA_CHECK(kMagic == (std::uint32_t{'K'} << 24 | std::uint32_t{'4'} << 16 | std::uint32_t{'O'} << 8 | std::uint32_t{'S'}));   // reads "K4OS"
}

static void test_message_type_table() {
    KRITVA_CHECK(kAllMessageTypes.size() == 23);
    std::set<std::uint16_t> values;
    std::set<std::string> names;
    std::uint16_t expected = 1;
    for (MessageType t : kAllMessageTypes) {
        const auto v = static_cast<std::uint16_t>(t);
        KRITVA_CHECK(v == expected++);                                       // contiguous 1..23 in order
        KRITVA_CHECK(values.insert(v).second && names.insert(message_type_name(t)).second);   // unique values and names
        KRITVA_CHECK(std::string(message_type_name(t)) != "INVALID" && message_type_from_wire(v) == t);
    }
    for (std::uint16_t bad : {std::uint16_t{0}, std::uint16_t{24}, std::uint16_t{0x0100}, std::uint16_t{0xFFFF}}) {
        KRITVA_CHECK(!message_type_from_wire(bad).has_value());
    }
    KRITVA_CHECK(std::string(message_type_name(static_cast<MessageType>(0x7777))) == "INVALID");
}

static void test_request_response_pairing() {
    int requests = 0, notices = 0;
    for (MessageType t : kAllMessageTypes) {
        if (is_request(t)) {
            ++requests;
            const auto r = response_for(t);
            KRITVA_CHECK(r.has_value() && static_cast<std::uint16_t>(*r) == static_cast<std::uint16_t>(t) + 1);
            KRITVA_CHECK(!is_request(*r) && !is_notice(*r) && direction_of(*r) == Direction::EDGE_TO_NEXUS);
            KRITVA_CHECK(direction_of(t) == Direction::NEXUS_TO_EDGE);
        } else {
            KRITVA_CHECK(!response_for(t).has_value());                      // responses and notices have no response
            if (is_notice(t)) ++notices;
        }
    }
    KRITVA_CHECK(requests == 10 && notices == 3);                            // HELLO, DISCOVERY, CONFIGURE, 4 lifecycle, READ, WRITE, OBSERVE
    KRITVA_CHECK(direction_of(MessageType::HEARTBEAT) == Direction::BOTH && direction_of(MessageType::PROTOCOL_ERROR) == Direction::BOTH);
    KRITVA_CHECK(direction_of(MessageType::FAULT_EVENT) == Direction::EDGE_TO_NEXUS);
    KRITVA_CHECK(*response_for(MessageType::READ_REQUEST) == MessageType::READ_RESPONSE);
    KRITVA_CHECK(*response_for(MessageType::HELLO) == MessageType::HELLO_ACK);
}

static void test_sequence_admission_by_kind_of_frame() {             // architect review: responses are admitted by correlation
    int response_like = 0, watermarked = 0;
    for (MessageType t : kAllMessageTypes) {
        if (t == MessageType::PROTOCOL_ERROR) continue;                       // depends on the correlation id, below
        if (is_response_like(t, 0)) ++response_like; else ++watermarked;
        KRITVA_CHECK(is_response_like(t, 0) == is_response_like(t, 99));      // for every other type the correlation id is irrelevant
        KRITVA_CHECK(is_response_like(t, 0) == (!is_request(t) && !is_notice(t)));
    }
    KRITVA_CHECK(response_like == 10 && watermarked == 12);                    // 10 responses (HELLO_ACK included); 10 requests + HEARTBEAT + FAULT_EVENT
    KRITVA_CHECK(is_response_like(MessageType::HELLO_ACK, 0) && is_response_like(MessageType::WRITE_RESPONSE, 7));
    KRITVA_CHECK(!is_response_like(MessageType::WRITE_REQUEST, 0) && !is_response_like(MessageType::HEARTBEAT, 0) &&
                 !is_response_like(MessageType::FAULT_EVENT, 0));
    KRITVA_CHECK(!is_response_like(MessageType::PROTOCOL_ERROR, 0) && is_response_like(MessageType::PROTOCOL_ERROR, 7));

    // The reordered-delivery scenario: WRITE_RESPONSE (10) sent before HEARTBEAT (11), delivered in the opposite order.
    std::uint64_t highest_notice = 0;
    KRITVA_CHECK(passes_stale_check(MessageType::HEARTBEAT, 0, 11, highest_notice));                 // the heartbeat is accepted first ...
    highest_notice = 11;
    KRITVA_CHECK(passes_stale_check(MessageType::WRITE_RESPONSE, 7, 10, highest_notice));            // ... and the response 10 is still admissible
    KRITVA_CHECK(passes_stale_check(MessageType::READ_RESPONSE, 3, 1, highest_notice));              // even a much lower sequence
    // Requests and notices keep the strict monotonic rule.
    KRITVA_CHECK(!passes_stale_check(MessageType::WRITE_REQUEST, 0, 10, highest_notice));            // a request 10 after 11 is stale
    KRITVA_CHECK(!passes_stale_check(MessageType::WRITE_REQUEST, 0, 11, highest_notice));            // equal: duplicate
    KRITVA_CHECK(passes_stale_check(MessageType::WRITE_REQUEST, 0, 12, highest_notice));
    KRITVA_CHECK(!passes_stale_check(MessageType::HEARTBEAT, 0, 11, highest_notice) && !passes_stale_check(MessageType::FAULT_EVENT, 0, 5, highest_notice));
    KRITVA_CHECK(!passes_stale_check(MessageType::PROTOCOL_ERROR, 0, 4, highest_notice));            // uncorrelated PROTOCOL_ERROR is a notice
    KRITVA_CHECK(passes_stale_check(MessageType::PROTOCOL_ERROR, 9, 4, highest_notice));             // correlated PROTOCOL_ERROR answers a request
}

static void test_version_negotiation() {
    const ProtocolVersion edge{1, 3};
    KRITVA_CHECK(negotiate(edge, {1, 3}) == ProtocolVersion(1, 3));          // equal: accepted
    KRITVA_CHECK(negotiate(edge, {1, 0}) == ProtocolVersion(1, 0));          // older minor: accepted, the peer's version is negotiated
    KRITVA_CHECK(!negotiate(edge, {1, 4}).has_value());                      // newer minor than supported
    KRITVA_CHECK(!negotiate(edge, {2, 0}).has_value() && !negotiate(edge, {0, 0}).has_value());   // different major
    KRITVA_CHECK(ProtocolVersion{}.major == 1 && ProtocolVersion{}.minor == 0);
    KRITVA_CHECK(negotiate({}, {}).has_value() && !negotiate({}, {1, 1}).has_value());
}

static void test_status_mapping() {
    for (std::uint32_t v = 0; v <= kMaxWireStatus; ++v) {
        const auto code = from_wire_status(static_cast<std::uint16_t>(v));
        KRITVA_CHECK(code.has_value() && to_wire_status(*code) == v);        // a bijection over 0..11
    }
    for (std::uint16_t bad : {std::uint16_t{12}, std::uint16_t{255}, std::uint16_t{0xFFFF}}) KRITVA_CHECK(!from_wire_status(bad).has_value());
    KRITVA_CHECK(to_wire_status(ErrorCode::UNSUPPORTED) == 10 && to_wire_status(ErrorCode::NONE) == 0);
}

static void test_session_transition_table() {                            // exhaustive: 5 x 5
    using S = SessionState;
    const S all[] = {S::DISCONNECTED, S::CONNECTING, S::NEGOTIATING, S::CONNECTED, S::DEGRADED};
    // expected[from][to]
    const bool expected[5][5] = {
        /* DISCONNECTED */ {false, true,  false, false, false},
        /* CONNECTING   */ {true,  false, true,  false, false},
        /* NEGOTIATING  */ {true,  false, false, true,  false},
        /* CONNECTED    */ {true,  false, false, false, true},
        /* DEGRADED     */ {true,  false, false, true,  false}};
    for (int f = 0; f < 5; ++f) for (int t = 0; t < 5; ++t) KRITVA_CHECK(is_valid_transition(all[f], all[t]) == expected[f][t]);
    for (S s : all) KRITVA_CHECK(!is_valid_transition(s, s));                // no self transition
    for (S s : {S::CONNECTING, S::NEGOTIATING, S::CONNECTED, S::DEGRADED}) KRITVA_CHECK(is_valid_transition(s, S::DISCONNECTED));   // every live state can drop
    KRITVA_CHECK(!is_valid_transition(S::DISCONNECTED, S::CONNECTED));       // the only way up is connect(): no shortcut to CONNECTED
    KRITVA_CHECK(!is_valid_transition(S::CONNECTING, S::CONNECTED) && !is_valid_transition(S::NEGOTIATING, S::DEGRADED));
    std::set<std::string> names;
    for (S s : all) KRITVA_CHECK(names.insert(session_state_name(s)).second);
    KRITVA_CHECK(std::string(session_state_name(S::DEGRADED)) == "DEGRADED");
}

static void test_link_timing() {
    LinkTiming t;                                                            // the documented defaults
    KRITVA_CHECK(t.heartbeat_period_ms == 100 && t.heartbeat_timeout_ms == 300 && t.request_timeout_ms == 100 && t.pump_quantum_ms == 1);
    KRITVA_CHECK(is_valid(t));
    KRITVA_CHECK(t.heartbeat_timeout_ms == 3 * t.heartbeat_period_ms);       // 3 periods
    auto with = [&](auto&& edit) { LinkTiming x; edit(x); return is_valid(x); };
    KRITVA_CHECK(with([](LinkTiming& x) { x.heartbeat_period_ms = 10; x.heartbeat_timeout_ms = 20; }));      // lowest legal
    KRITVA_CHECK(!with([](LinkTiming& x) { x.heartbeat_period_ms = 9; }));
    KRITVA_CHECK(with([](LinkTiming& x) { x.heartbeat_period_ms = 60000; x.heartbeat_timeout_ms = 120000; }));
    KRITVA_CHECK(!with([](LinkTiming& x) { x.heartbeat_period_ms = 60001; x.heartbeat_timeout_ms = 200000; }));
    KRITVA_CHECK(!with([](LinkTiming& x) { x.heartbeat_timeout_ms = 199; }));          // less than 2 periods
    KRITVA_CHECK(with([](LinkTiming& x) { x.heartbeat_timeout_ms = 200; }));           // exactly 2 periods
    KRITVA_CHECK(!with([](LinkTiming& x) { x.heartbeat_timeout_ms = 600001; }));
    KRITVA_CHECK(!with([](LinkTiming& x) { x.heartbeat_timeout_ms = 19; x.heartbeat_period_ms = 10; }));
    KRITVA_CHECK(!with([](LinkTiming& x) { x.request_timeout_ms = 0; }) && !with([](LinkTiming& x) { x.request_timeout_ms = 60001; }));
    KRITVA_CHECK(!with([](LinkTiming& x) { x.pump_quantum_ms = 0; }) && !with([](LinkTiming& x) { x.pump_quantum_ms = 1001; }));
    KRITVA_CHECK(!with([](LinkTiming& x) { x.pump_quantum_ms = 50; x.request_timeout_ms = 49; }));   // quantum above the request timeout
    KRITVA_CHECK(with([](LinkTiming& x) { x.pump_quantum_ms = 50; x.request_timeout_ms = 50; }));
    KRITVA_CHECK(!with([](LinkTiming& x) { x.heartbeat_period_ms = 0xFFFFFFFF; }));    // no overflow through the 2x check
}

static void test_key_names() {
    KRITVA_CHECK(std::string(kKeyHeartbeatPeriodMs) == "link.heartbeat_period_ms" && std::string(kKeyHeartbeatTimeoutMs) == "link.heartbeat_timeout_ms");
    KRITVA_CHECK(std::string(kKeyRequestTimeoutMs) == "link.request_timeout_ms" && std::string(kKeyPumpQuantumMs) == "link.pump_quantum_ms");
}

static void test_limits_are_consistent() {
    KRITVA_CHECK(kMaxPayloadSize + kHeaderSize == kMaxFrameSize);
    KRITVA_CHECK(kMaxNameLength == 64 && kMaxTextLength == 256 && kMaxDevices == 64 && kMaxDiscoveryItems == 256);
    KRITVA_CHECK(kMaxCapabilitiesPerEndpoint == 16 && kMaxConfigureSettings == 16);
}

static void test_discovery_capacity() {                                  // frame bound versus item limits
    // Protocol 1.0 needs exactly one capability per endpoint, so the largest legal snapshot is 43524 bytes and fits one frame.
    KRITVA_CHECK(kMaxDiscoveryPayload == 43524);
    KRITVA_CHECK(kMaxDiscoveryPayload < kMaxPayloadSize);
    KRITVA_CHECK(kMaxPayloadSize - kMaxDiscoveryPayload == 21968);               // the remaining headroom
    // The wire maximum (16 capabilities per endpoint) is a different matter: it could not fit, which is why the frame bound is the
    // hard limit and the item limits are semantic ones that an encoder checks against the frame size first.
    const std::size_t worst_on_the_wire = 2 + 2 + kMaxDevices * (8 + 2 + kMaxNameLength + 2) +
        kMaxDiscoveryItems * (8 + 2 + kMaxNameLength + 1 + 2 + kMaxCapabilitiesPerEndpoint * (8 + 2 + kMaxNameLength));
    KRITVA_CHECK(worst_on_the_wire > kMaxPayloadSize);
}

int main() {
    test_magic_on_the_wire();
    test_message_type_table();
    test_request_response_pairing();
    test_sequence_admission_by_kind_of_frame();
    test_version_negotiation();
    test_status_mapping();
    test_session_transition_table();
    test_link_timing();
    test_key_names();
    test_limits_are_consistent();
    test_discovery_capacity();
    std::printf("protocol_contract_test: PASS\n");
    return 0;
}
