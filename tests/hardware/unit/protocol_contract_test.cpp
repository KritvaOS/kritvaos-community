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
    // The largest legal discovery response still fits in one frame: 256 endpoints x (8 + 2 + 64 + 1 + 2 + 16 x (8 + 2 + 64)) bytes is
    // too large, so the capability limit and the endpoint limit are not both reachable at once; the codec (I4-002) enforces the frame bound.
    const std::size_t worst_endpoint = 8 + 2 + kMaxNameLength + 1 + 2 + kMaxCapabilitiesPerEndpoint * (8 + 2 + kMaxNameLength);
    KRITVA_CHECK(worst_endpoint * kMaxDiscoveryItems > kMaxPayloadSize);       // documented: the frame limit binds before the item limit in the worst case
}

int main() {
    test_magic_on_the_wire();
    test_message_type_table();
    test_request_response_pairing();
    test_version_negotiation();
    test_status_mapping();
    test_session_transition_table();
    test_link_timing();
    test_key_names();
    test_limits_are_consistent();
    std::printf("protocol_contract_test: PASS\n");
    return 0;
}
