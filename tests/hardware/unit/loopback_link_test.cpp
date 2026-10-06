//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : loopback_link_test.cpp
// Description : Unit tests of the LoopbackLink: exact consumption of virtual time, the fixed driving order, determinism and
//               the absence of any state or decision of its own.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: RI-001; TR-001
// API         : LOOPBACK-LINK-UT
//
// Author      : KritvaOS
// Created     : 06-10-2026
//==============================================================================

#include "../support/stack_rig.hpp"

using namespace kritva::hardware::remote;
using namespace kritva::hardware::remote::test;
using kritva::core::ErrorCode;

static constexpr std::uint64_t kMs = 1'000'000;

static std::vector<Reply> heartbeats(const std::vector<Reply>& all) {
    std::vector<Reply> out;
    for (const Reply& r : all) if (r.header.type == MessageType::HEARTBEAT) out.push_back(r);
    return out;
}

static void test_advance_consumes_exactly_the_requested_time() {
    EdgeRig rig;
    LoopbackLink loop(rig.link, *rig.edge);
    KRITVA_CHECK(loop.quantum_ns() == kMs && loop.now_ns() == 0);
    KRITVA_CHECK(loop.advance(0).has_value() && rig.link.now_ns() == 0);                 // nothing
    KRITVA_CHECK(loop.advance(10 * kMs).has_value() && rig.link.now_ns() == 10 * kMs && loop.now_ns() == 10 * kMs);
    KRITVA_CHECK(loop.advance(1).has_value() && rig.link.now_ns() == 10 * kMs + 1);      // less than one quantum
    KRITVA_CHECK(loop.advance(2 * kMs + 7).has_value() && rig.link.now_ns() == 12 * kMs + 8);
}

static void test_the_quantum_sets_the_granularity_of_driving() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    LoopbackLink loop(rig.link, *rig.edge, 3 * kMs);                                     // 3 ms steps
    KRITVA_CHECK(loop.advance(100 * kMs).has_value() && rig.link.now_ns() == 100 * kMs);
    // The Edge's heartbeat is due at 100 ms, but it is only polled at the steps 99 ms and 100 ms (the last, shorter step).
    const auto hb = heartbeats(rig.drain());
    KRITVA_CHECK(hb.size() == 1 && decode<HeartbeatPayload>(hb[0].payload).value.sender_time_ns == 100 * kMs);
    EdgeRig rig2;
    KRITVA_CHECK(rig2.hello().status.ok());
    LoopbackLink coarse(rig2.link, *rig2.edge, 30 * kMs);
    KRITVA_CHECK(coarse.advance(90 * kMs).has_value());
    KRITVA_CHECK(heartbeats(rig2.drain()).empty());                                      // polled at 30, 60 and 90 ms: the heartbeat is not due yet
    KRITVA_CHECK(coarse.advance(30 * kMs).has_value());                                  // the next poll is at 120 ms, and that is when it is sent
    const auto late = heartbeats(rig2.drain());
    KRITVA_CHECK(late.size() == 1 && decode<HeartbeatPayload>(late[0].payload).value.sender_time_ns == 120 * kMs);
}

static void test_a_zero_quantum_means_the_default() {
    EdgeRig rig;
    LoopbackLink loop(rig.link, *rig.edge, 0);
    KRITVA_CHECK(loop.quantum_ns() == LoopbackLink::kDefaultQuantumNs);
}

// The order of a step is fixed: the clock moves, the Edge handles what is due and supervises, then the Nexus reads what is due
// (the Edge's answers included) and supervises.
static void test_the_order_of_a_step_is_edge_then_nexus() {
    StackRig rig;
    rig.up();
    const auto nexus_sent = rig.session().stats().heartbeats_sent;
    const auto nexus_notices = rig.session().stats().notices;
    const auto edge_got = rig.edge().stats().heartbeats;
    const auto edge_sent = rig.edge().stats().heartbeats_sent;
    KRITVA_CHECK(rig.loop.advance(100 * kMs).has_value());
    // At 100 ms: the Edge polled first and sent its heartbeat; the Nexus, serviced after it in the same step, read it. The Nexus
    // then sent its own, which the Edge can only read in the next step.
    KRITVA_CHECK(rig.edge().stats().heartbeats_sent - edge_sent == 1 && rig.session().stats().notices - nexus_notices == 1);
    KRITVA_CHECK(rig.session().stats().heartbeats_sent - nexus_sent == 1 && rig.edge().stats().heartbeats - edge_got == 0);
    KRITVA_CHECK(rig.loop.advance(1 * kMs).has_value());
    KRITVA_CHECK(rig.edge().stats().heartbeats - edge_got == 1);
}

static void test_pump_moves_no_time_and_is_idempotent_without_work() {
    StackRig rig;
    rig.up();
    const auto now = rig.link().now_ns();
    const EdgeStats before = rig.edge().stats();
    rig.loop.pump();
    rig.loop.pump();
    KRITVA_CHECK(rig.link().now_ns() == now && rig.edge().stats().frames_received == before.frames_received && rig.edge().stats().heartbeats_sent == before.heartbeats_sent);
}

static void test_without_a_node_only_the_edge_is_driven_and_attach_adds_the_nexus() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    LoopbackLink loop(rig.link, *rig.edge);
    KRITVA_CHECK(loop.advance(400 * kMs).has_value());                                   // nobody feeds the Edge: it times out by itself
    KRITVA_CHECK(!rig.edge->session().valid() && rig.stats().sessions_timed_out == 1);
    // With the Nexus attached it is supervised too.
    StackRig stack;
    stack.up();
    const auto sent = stack.session().stats().heartbeats_sent;
    KRITVA_CHECK(stack.loop.advance(100 * kMs).has_value() && stack.session().stats().heartbeats_sent > sent);
}

static void test_the_peer_tick_polls_the_edge() {
    EdgeRig rig;
    LoopbackLink loop(rig.link, *rig.edge);
    rig.send_raw(EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 1, 0, 100, 300})), 1, 0));
    KRITVA_CHECK(!rig.edge->session().valid());
    loop.peer_tick()();
    KRITVA_CHECK(rig.edge->session().valid());
}

static void test_the_clock_overflow_is_refused() {
    EdgeRig rig;
    KRITVA_CHECK(rig.link.advance(UINT64_MAX - 5).has_value());
    LoopbackLink loop(rig.link, *rig.edge, 4);
    const auto r = loop.advance(10);                                                     // 4 fits, the next 4 does not
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT && rig.link.now_ns() == UINT64_MAX - 1);
}

// The same script over the same link gives the same behavior; a different link configuration gives a different one.
static std::vector<std::uint64_t> scripted(std::uint64_t seed) {
    StackRig rig;
    rig.up();
    DirectionConfig c;
    c.latency_ns = 2 * kMs;
    c.seed = seed;
    c.drop_permille = 300;
    c.duplicate_permille = 200;
    c.reorder_permille = 200;
    c.reorder_delay_ns = 9 * kMs;
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, c).has_value());
    KRITVA_CHECK(rig.link().configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    std::vector<std::uint64_t> log;
    for (int i = 0; i < 30; ++i) {
        KRITVA_CHECK(rig.loop.advance(37 * kMs).has_value());
        log.push_back(static_cast<std::uint64_t>(rig.session().state()));
        log.push_back(rig.session().stats().notices);
        log.push_back(rig.edge().stats().heartbeats);
        if (rig.session().state() == SessionState::DISCONNECTED) break;
    }
    log.push_back(rig.link().now_ns());
    return log;
}

static void test_repeatability() {
    const auto a = scripted(5);
    KRITVA_CHECK(a == scripted(5) && a.size() > 10);
    bool differs = false;
    for (std::uint64_t seed = 6; seed < 14 && !differs; ++seed) differs = scripted(seed) != a;
    KRITVA_CHECK(differs);
}

int main() {
    test_advance_consumes_exactly_the_requested_time();
    test_the_quantum_sets_the_granularity_of_driving();
    test_a_zero_quantum_means_the_default();
    test_the_order_of_a_step_is_edge_then_nexus();
    test_pump_moves_no_time_and_is_idempotent_without_work();
    test_without_a_node_only_the_edge_is_driven_and_attach_adds_the_nexus();
    test_the_peer_tick_polls_the_edge();
    test_the_clock_overflow_is_refused();
    test_repeatability();
    std::printf("loopback_link_test: PASS\n");
    return 0;
}
