//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : simulated_transport_test.cpp
// Description : Unit tests of the deterministic simulated transport: clock, latency, ordering, drop,
//               reorder, duplicate, disconnect, bounds and repeatability.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: TR-001; TR-002
// API         : SIMULATED-TRANSPORT-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <cstdint>
#include <string>
#include <vector>

#include "../../runtime/check.hpp"

#include <kritva/hardware/transport/simulated_transport.hpp>

using namespace kritva::hardware::transport;
using kritva::core::ErrorCode;
using Bytes = std::vector<std::uint8_t>;

static Bytes frame(std::uint8_t tag, std::size_t size = 8) { return Bytes(size, tag); }
static bool is(const std::optional<Bytes>& got, std::uint8_t tag) { return got && !got->empty() && (*got)[0] == tag; }

struct Link {
    SimulatedTransport t;
    Link() { KRITVA_CHECK(t.connect().has_value()); }
};

static void test_initial_state_and_clock() {                         // TR-001
    SimulatedTransport t;
    KRITVA_CHECK(t.link_state() == LinkState::DISCONNECTED && t.now_ns() == 0);
    KRITVA_CHECK(t.nexus().link_state() == LinkState::DISCONNECTED && t.edge().now_ns() == 0);
    KRITVA_CHECK(!t.step());                                             // nothing in flight: the clock does not move
    KRITVA_CHECK(t.now_ns() == 0);
    KRITVA_CHECK(t.advance(0).has_value() && t.now_ns() == 0);           // zero is allowed
    KRITVA_CHECK(t.nexus().advance(5).has_value() && t.now_ns() == 5 && t.edge().now_ns() == 5);   // one shared clock
    KRITVA_CHECK(t.edge().advance(7).has_value() && t.now_ns() == 12);
    KRITVA_CHECK(t.advance(UINT64_MAX).error().code == ErrorCode::INVALID_ARGUMENT && t.now_ns() == 12);   // overflow refused, unchanged
    KRITVA_CHECK(t.advance(UINT64_MAX - 12).has_value() && t.now_ns() == UINT64_MAX);                      // exactly to the limit
    KRITVA_CHECK(!t.advance(1).has_value() && t.now_ns() == UINT64_MAX);
    KRITVA_CHECK(t.advance(0).has_value());
}

static void test_time_never_moves_by_itself() {                       // TR-001: no hidden activity
    Link l;
    KRITVA_CHECK(l.t.nexus().send(frame(1)).has_value());
    for (int i = 0; i < 1000; ++i) {                                     // receive/pending/stats/link_state never advance time
        (void)l.t.pending(LinkDirection::NEXUS_TO_EDGE);
        (void)l.t.stats(LinkDirection::NEXUS_TO_EDGE);
        (void)l.t.link_state();
    }
    KRITVA_CHECK(l.t.now_ns() == 0);
    KRITVA_CHECK(is(l.t.edge().receive(), 1) && l.t.now_ns() == 0);
    KRITVA_CHECK(!l.t.edge().receive());                                 // empty receive does not move time either
    KRITVA_CHECK(l.t.now_ns() == 0);
}

static void test_zero_latency() {                                     // TR-002
    Link l;
    KRITVA_CHECK(l.t.nexus().send(frame(1)).has_value());
    KRITVA_CHECK(l.t.pending(LinkDirection::NEXUS_TO_EDGE) == 1);
    KRITVA_CHECK(is(l.t.edge().receive(), 1));                           // receivable at once, without advancing
    KRITVA_CHECK(l.t.pending(LinkDirection::NEXUS_TO_EDGE) == 0 && !l.t.edge().receive());
    KRITVA_CHECK(!l.t.nexus().receive());                                // the other direction is separate
}

static void test_fixed_latency_boundaries() {                         // TR-002: no delivery before due time, delivery exactly at it
    Link l;
    DirectionConfig c;
    c.latency_ns = 1000;
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    KRITVA_CHECK(l.t.advance(500).has_value());
    KRITVA_CHECK(l.t.nexus().send(frame(1)).has_value());                // sent at 500, due at 1500
    KRITVA_CHECK(!l.t.edge().receive());
    KRITVA_CHECK(l.t.advance(999).has_value() && l.t.now_ns() == 1499);
    KRITVA_CHECK(!l.t.edge().receive());                                 // one ns early
    KRITVA_CHECK(l.t.advance(1).has_value() && l.t.now_ns() == 1500);
    KRITVA_CHECK(is(l.t.edge().receive(), 1));                           // exactly at the due time
    KRITVA_CHECK(!l.t.edge().receive());
    KRITVA_CHECK(l.t.advance(10000).has_value());
    KRITVA_CHECK(!l.t.edge().receive());                                 // delivered at most once

    // The other direction keeps its own (zero) latency.
    KRITVA_CHECK(l.t.edge().send(frame(2)).has_value() && is(l.t.nexus().receive(), 2));
}

static void test_step_jumps_to_the_next_due_time() {                  // TR-001: step() boundary behavior
    Link l;
    DirectionConfig a;
    a.latency_ns = 300;
    DirectionConfig b;
    b.latency_ns = 100;
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, a).has_value() && l.t.configure(LinkDirection::EDGE_TO_NEXUS, b).has_value());
    KRITVA_CHECK(l.t.nexus().send(frame(1)).has_value());                // due 300
    KRITVA_CHECK(l.t.edge().send(frame(2)).has_value());                 // due 100
    KRITVA_CHECK(l.t.step() && l.t.now_ns() == 100);                     // the earliest of both directions
    KRITVA_CHECK(!l.t.edge().receive() && is(l.t.nexus().receive(), 2));
    KRITVA_CHECK(l.t.step() && l.t.now_ns() == 300);
    KRITVA_CHECK(is(l.t.edge().receive(), 1));
    KRITVA_CHECK(!l.t.step() && l.t.now_ns() == 300);                    // idle again
    // step() with something already receivable does not move the clock.
    KRITVA_CHECK(l.t.advance(50).has_value());
    DirectionConfig z;
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, z).has_value());
    KRITVA_CHECK(l.t.nexus().send(frame(3)).has_value());
    const std::uint64_t before = l.t.now_ns();
    KRITVA_CHECK(l.t.step() && l.t.now_ns() == before);
    KRITVA_CHECK(is(l.t.edge().receive(), 3));
}

static void test_ordering_and_equal_due_times() {                     // TR-002
    Link l;
    for (std::uint8_t i = 1; i <= 5; ++i) KRITVA_CHECK(l.t.nexus().send(frame(i)).has_value());
    for (std::uint8_t i = 1; i <= 5; ++i) KRITVA_CHECK(is(l.t.edge().receive(), i));   // equal due times: send order
    KRITVA_CHECK(!l.t.edge().receive());

    DirectionConfig c;
    c.latency_ns = 100;
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    KRITVA_CHECK(l.t.nexus().send(frame(10)).has_value());               // due 100
    KRITVA_CHECK(l.t.advance(40).has_value());
    KRITVA_CHECK(l.t.nexus().send(frame(11)).has_value());               // due 140
    KRITVA_CHECK(l.t.advance(1000).has_value());
    KRITVA_CHECK(is(l.t.edge().receive(), 10) && is(l.t.edge().receive(), 11) && !l.t.edge().receive());
}

static void test_explicit_drop_and_stats() {                          // TR-002
    Link l;
    DirectionConfig c;
    c.faults[1].drop = true;                                             // the second accepted frame
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    for (std::uint8_t i = 1; i <= 4; ++i) KRITVA_CHECK(l.t.nexus().send(frame(i)).has_value());   // a drop is not an error to the sender
    KRITVA_CHECK(is(l.t.edge().receive(), 1) && is(l.t.edge().receive(), 3) && is(l.t.edge().receive(), 4) && !l.t.edge().receive());
    const auto& s = l.t.stats(LinkDirection::NEXUS_TO_EDGE);
    KRITVA_CHECK(s.sent == 4 && s.dropped == 1 && s.delivered == 3 && s.duplicated == 0);
    KRITVA_CHECK(l.t.stats(LinkDirection::EDGE_TO_NEXUS).sent == 0);
}

static void test_explicit_reorder() {                                 // TR-002
    Link l;
    DirectionConfig c;
    c.latency_ns = 10;
    c.faults[0].extra_delay_ns = 100;                                    // the first frame is held back
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    for (std::uint8_t i = 1; i <= 3; ++i) KRITVA_CHECK(l.t.nexus().send(frame(i)).has_value());
    KRITVA_CHECK(l.t.advance(10).has_value());
    KRITVA_CHECK(is(l.t.edge().receive(), 2) && is(l.t.edge().receive(), 3) && !l.t.edge().receive());   // overtaken
    KRITVA_CHECK(l.t.advance(100).has_value());
    KRITVA_CHECK(is(l.t.edge().receive(), 1));
}

static void test_explicit_duplicate() {                               // TR-002
    Link l;
    DirectionConfig c;
    c.latency_ns = 10;
    c.faults[0].duplicate = true;
    c.faults[0].duplicate_extra_delay_ns = 50;
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    KRITVA_CHECK(l.t.nexus().send(frame(1)).has_value() && l.t.nexus().send(frame(2)).has_value());
    KRITVA_CHECK(l.t.advance(10).has_value());
    KRITVA_CHECK(is(l.t.edge().receive(), 1) && is(l.t.edge().receive(), 2) && !l.t.edge().receive());
    KRITVA_CHECK(l.t.advance(49).has_value() && !l.t.edge().receive());
    KRITVA_CHECK(l.t.advance(1).has_value());
    const auto copy = l.t.edge().receive();
    KRITVA_CHECK(is(copy, 1) && *copy == frame(1));                      // the copy is byte-identical and late
    const auto& s = l.t.stats(LinkDirection::NEXUS_TO_EDGE);
    KRITVA_CHECK(s.sent == 2 && s.duplicated == 1 && s.delivered == 3);
}

static void test_fault_on_drop_wins_over_duplicate() {
    Link l;
    DirectionConfig c;
    c.faults[0].drop = true;
    c.faults[0].duplicate = true;
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    KRITVA_CHECK(l.t.nexus().send(frame(1)).has_value());
    KRITVA_CHECK(!l.t.edge().receive() && l.t.stats(LinkDirection::NEXUS_TO_EDGE).duplicated == 0);
}

static void test_disconnect_and_reconnect() {                         // TR-002
    Link l;
    DirectionConfig c;
    c.latency_ns = 100;
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value() && l.t.configure(LinkDirection::EDGE_TO_NEXUS, c).has_value());
    KRITVA_CHECK(l.t.nexus().send(frame(1)).has_value() && l.t.nexus().send(frame(2)).has_value());
    KRITVA_CHECK(l.t.edge().send(frame(3)).has_value());
    l.t.nexus().disconnect();                                            // either end can take the link down
    KRITVA_CHECK(l.t.link_state() == LinkState::DISCONNECTED && l.t.edge().link_state() == LinkState::DISCONNECTED);
    KRITVA_CHECK(l.t.pending(LinkDirection::NEXUS_TO_EDGE) == 0 && l.t.pending(LinkDirection::EDGE_TO_NEXUS) == 0);
    KRITVA_CHECK(l.t.stats(LinkDirection::NEXUS_TO_EDGE).discarded_on_disconnect == 2);
    KRITVA_CHECK(l.t.stats(LinkDirection::EDGE_TO_NEXUS).discarded_on_disconnect == 1);
    KRITVA_CHECK(l.t.advance(1000).has_value() && !l.t.edge().receive() && !l.t.nexus().receive());   // nothing survives
    const auto r = l.t.nexus().send(frame(4));
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::RESOURCE_UNAVAILABLE);
    KRITVA_CHECK(l.t.edge().send(frame(5)).error().code == ErrorCode::RESOURCE_UNAVAILABLE);
    KRITVA_CHECK(l.t.stats(LinkDirection::NEXUS_TO_EDGE).rejected_link_down == 1 && l.t.stats(LinkDirection::NEXUS_TO_EDGE).sent == 2);
    l.t.disconnect();                                                    // idempotent: nothing counted twice
    KRITVA_CHECK(l.t.stats(LinkDirection::NEXUS_TO_EDGE).discarded_on_disconnect == 2);

    KRITVA_CHECK(l.t.edge().connect().has_value() && l.t.link_state() == LinkState::CONNECTED);
    KRITVA_CHECK(l.t.connect().has_value());                             // idempotent
    KRITVA_CHECK(!l.t.edge().receive());                                 // old frames never come back
    KRITVA_CHECK(l.t.nexus().send(frame(6)).has_value());
    KRITVA_CHECK(l.t.advance(100).has_value() && is(l.t.edge().receive(), 6));
}

static void test_disconnect_is_connectivity_only() {                  // TR-002: no protocol or safety decision
    Link l;
    l.t.disconnect();
    KRITVA_CHECK(l.t.now_ns() == 0);                                     // does not touch time
    KRITVA_CHECK(l.t.stats(LinkDirection::NEXUS_TO_EDGE).sent == 0 && l.t.stats(LinkDirection::EDGE_TO_NEXUS).sent == 0);
    KRITVA_CHECK(l.t.nexus().send(frame(1)).error().code == ErrorCode::RESOURCE_UNAVAILABLE);   // nothing is generated or queued
    KRITVA_CHECK(l.t.pending(LinkDirection::NEXUS_TO_EDGE) == 0 && l.t.pending(LinkDirection::EDGE_TO_NEXUS) == 0);
}

static void test_frame_size_bounds() {
    Link l;
    KRITVA_CHECK(l.t.nexus().send(Bytes{}).error().code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(l.t.nexus().send(frame(1, 65537)).error().code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(l.t.nexus().send(frame(1, 1)).has_value());
    KRITVA_CHECK(l.t.nexus().send(frame(2, 65536)).has_value());
    const auto a = l.t.edge().receive();
    const auto b = l.t.edge().receive();
    KRITVA_CHECK(a && a->size() == 1 && b && b->size() == 65536);
    const auto& s = l.t.stats(LinkDirection::NEXUS_TO_EDGE);
    KRITVA_CHECK(s.rejected_invalid == 2 && s.sent == 2);                // refused frames count neither as sent nor consume an index
}

static void test_refused_sends_consume_no_fault_index() {
    Link l;
    DirectionConfig c;
    c.faults[0].drop = true;
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    KRITVA_CHECK(!l.t.nexus().send(Bytes{}).has_value());                // refused: the fault still waits for the first accepted frame
    KRITVA_CHECK(l.t.nexus().send(frame(1)).has_value() && l.t.nexus().send(frame(2)).has_value());
    KRITVA_CHECK(is(l.t.edge().receive(), 2) && !l.t.edge().receive());
}

static void test_queue_capacity() {
    Link l;
    DirectionConfig c;
    c.queue_capacity = 2;
    c.latency_ns = 10;
    c.faults[0].duplicate = true;                                        // would need a third slot
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    KRITVA_CHECK(l.t.nexus().send(frame(1)).has_value());                // 1 in flight, plus its duplicate
    KRITVA_CHECK(l.t.pending(LinkDirection::NEXUS_TO_EDGE) == 2);
    const auto full = l.t.nexus().send(frame(2));
    KRITVA_CHECK(!full.has_value() && full.error().code == ErrorCode::RESOURCE_UNAVAILABLE);
    KRITVA_CHECK(l.t.stats(LinkDirection::NEXUS_TO_EDGE).rejected_queue_full == 1);
    KRITVA_CHECK(l.t.pending(LinkDirection::NEXUS_TO_EDGE) == 2);            // the bound holds
    KRITVA_CHECK(l.t.advance(10).has_value() && l.t.edge().receive() && l.t.edge().receive());
    KRITVA_CHECK(l.t.nexus().send(frame(3)).has_value());                // room again after delivery

    // A duplicate that does not fit is not made.
    DirectionConfig d;
    d.queue_capacity = 1;
    d.faults[0].duplicate = true;
    KRITVA_CHECK(l.t.configure(LinkDirection::EDGE_TO_NEXUS, d).has_value());
    KRITVA_CHECK(l.t.edge().send(frame(9)).has_value());
    KRITVA_CHECK(l.t.pending(LinkDirection::EDGE_TO_NEXUS) == 1 && l.t.stats(LinkDirection::EDGE_TO_NEXUS).duplicated == 0);
}

static void test_configure_validation() {
    SimulatedTransport t;
    DirectionConfig c;
    c.queue_capacity = 0;
    KRITVA_CHECK(t.configure(LinkDirection::NEXUS_TO_EDGE, c).error().code == ErrorCode::INVALID_ARGUMENT);
    c.queue_capacity = 65537;
    KRITVA_CHECK(!t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    c = {};
    c.drop_permille = 1001;
    KRITVA_CHECK(!t.configure(LinkDirection::EDGE_TO_NEXUS, c).has_value());
    c = {};
    c.duplicate_permille = 1001;
    KRITVA_CHECK(!t.configure(LinkDirection::EDGE_TO_NEXUS, c).has_value());
    c = {};
    c.reorder_permille = 1001;
    KRITVA_CHECK(!t.configure(LinkDirection::EDGE_TO_NEXUS, c).has_value());
    c = {};
    c.queue_capacity = 65536;
    c.drop_permille = c.duplicate_permille = c.reorder_permille = 1000;
    KRITVA_CHECK(t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());   // boundaries are valid
}

static void test_extreme_delays_saturate() {
    Link l;
    DirectionConfig c;
    c.latency_ns = UINT64_MAX;
    c.faults[0].extra_delay_ns = UINT64_MAX;
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    KRITVA_CHECK(l.t.advance(5).has_value());
    KRITVA_CHECK(l.t.nexus().send(frame(1)).has_value());                // due saturates at the maximum, never wraps to the past
    KRITVA_CHECK(!l.t.edge().receive());
    KRITVA_CHECK(l.t.step() && l.t.now_ns() == UINT64_MAX);
    KRITVA_CHECK(is(l.t.edge().receive(), 1));
}

static void test_probabilistic_extremes() {
    Link l;
    DirectionConfig all;
    all.drop_permille = 1000;
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, all).has_value());
    for (int i = 0; i < 50; ++i) KRITVA_CHECK(l.t.nexus().send(frame(1)).has_value());
    KRITVA_CHECK(!l.t.edge().receive() && l.t.stats(LinkDirection::NEXUS_TO_EDGE).dropped == 50);

    DirectionConfig none;                                                // 0 permille never triggers
    none.seed = 99;
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, none).has_value());
    for (int i = 0; i < 200; ++i) KRITVA_CHECK(l.t.nexus().send(frame(2)).has_value());
    KRITVA_CHECK(l.t.stats(LinkDirection::NEXUS_TO_EDGE).dropped == 50 && l.t.pending(LinkDirection::NEXUS_TO_EDGE) == 200);

    DirectionConfig dup;
    dup.duplicate_permille = 1000;
    dup.queue_capacity = 65536;
    KRITVA_CHECK(l.t.configure(LinkDirection::EDGE_TO_NEXUS, dup).has_value());
    for (int i = 0; i < 10; ++i) KRITVA_CHECK(l.t.edge().send(frame(3)).has_value());
    KRITVA_CHECK(l.t.pending(LinkDirection::EDGE_TO_NEXUS) == 20 && l.t.stats(LinkDirection::EDGE_TO_NEXUS).duplicated == 10);
}

static void test_probability_boundaries_and_reorder_delay() {          // TR-002
    Link l;
    DirectionConfig none;                                                // 0 permille never triggers, over many draws
    none.queue_capacity = 65536;
    none.seed = 7;
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, none).has_value());
    for (int i = 0; i < 20000; ++i) KRITVA_CHECK(l.t.nexus().send(frame(1, 1)).has_value());
    const auto& s = l.t.stats(LinkDirection::NEXUS_TO_EDGE);
    KRITVA_CHECK(s.dropped == 0 && s.duplicated == 0 && l.t.pending(LinkDirection::NEXUS_TO_EDGE) == 20000);

    DirectionConfig some;                                                // a small permille does act, and not on every frame
    some.queue_capacity = 65536;
    some.drop_permille = 100;
    KRITVA_CHECK(l.t.configure(LinkDirection::EDGE_TO_NEXUS, some).has_value());
    for (int i = 0; i < 20000; ++i) KRITVA_CHECK(l.t.edge().send(frame(1, 1)).has_value());
    const auto d = l.t.stats(LinkDirection::EDGE_TO_NEXUS).dropped;
    KRITVA_CHECK(d > 1500 && d < 2500);                                  // about 10 percent of 20000

    // A reordered frame is held back by exactly reorder_delay_ns; with permille 0 no frame is.
    SimulatedTransport t;
    KRITVA_CHECK(t.connect().has_value());
    DirectionConfig r;
    r.latency_ns = 10;
    r.reorder_permille = 1000;
    r.reorder_delay_ns = 90;
    KRITVA_CHECK(t.configure(LinkDirection::NEXUS_TO_EDGE, r).has_value());
    KRITVA_CHECK(t.nexus().send(frame(1)).has_value());
    KRITVA_CHECK(t.advance(99).has_value() && !t.edge().receive());
    KRITVA_CHECK(t.advance(1).has_value() && is(t.edge().receive(), 1));
    r.reorder_permille = 0;
    KRITVA_CHECK(t.configure(LinkDirection::NEXUS_TO_EDGE, r).has_value());
    KRITVA_CHECK(t.nexus().send(frame(2)).has_value());
    KRITVA_CHECK(t.advance(10).has_value() && is(t.edge().receive(), 2));
}

static void test_configure_restarts_the_send_index() {
    Link l;
    KRITVA_CHECK(l.t.nexus().send(frame(1)).has_value() && l.t.nexus().send(frame(2)).has_value());
    KRITVA_CHECK(l.t.edge().receive() && l.t.edge().receive());
    DirectionConfig c;
    c.faults[0].drop = true;                                             // refers to the first send after this configure
    KRITVA_CHECK(l.t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    KRITVA_CHECK(l.t.nexus().send(frame(3)).has_value() && l.t.nexus().send(frame(4)).has_value());
    KRITVA_CHECK(is(l.t.edge().receive(), 4) && !l.t.edge().receive());
}

// A recorded run: every frame the link delivered, with the clock at that moment.
struct Observation {
    std::uint64_t at;
    Bytes bytes;
    bool operator==(const Observation&) const = default;
};

static std::vector<Observation> scripted_run(std::uint64_t seed) {
    SimulatedTransport t;
    DirectionConfig c;
    c.latency_ns = 1000;
    c.seed = seed;
    c.drop_permille = 200;
    c.duplicate_permille = 200;
    c.reorder_permille = 300;
    c.reorder_delay_ns = 5000;
    c.faults[7].extra_delay_ns = 123;
    KRITVA_CHECK(t.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value() && t.configure(LinkDirection::EDGE_TO_NEXUS, c).has_value());
    KRITVA_CHECK(t.connect().has_value());
    std::vector<Observation> out;
    for (std::uint8_t i = 0; i < 100; ++i) {
        KRITVA_CHECK(t.nexus().send(frame(i, 1 + i % 9)).has_value());
        KRITVA_CHECK(t.edge().send(frame(static_cast<std::uint8_t>(200 - i), 3)).has_value());
        KRITVA_CHECK(t.advance(700).has_value());
        while (auto f = t.edge().receive()) out.push_back({t.now_ns(), *f});
        while (auto f = t.nexus().receive()) out.push_back({t.now_ns() + 1, *f});
    }
    while (t.step()) {
        while (auto f = t.edge().receive()) out.push_back({t.now_ns(), *f});
        while (auto f = t.nexus().receive()) out.push_back({t.now_ns() + 1, *f});
    }
    return out;
}

static void test_repeatability_and_seed_sensitivity() {               // TR-001/TR-002: same configuration, same behavior
    const auto a = scripted_run(42);
    const auto b = scripted_run(42);
    KRITVA_CHECK(!a.empty() && a == b);                                  // identical, including delivery times
    KRITVA_CHECK(scripted_run(43) != a);                                 // a different seed changes the behavior
    KRITVA_CHECK(scripted_run(0) == scripted_run(1));                    // documented: 0 is treated as 1
    KRITVA_CHECK(a.size() > 100 && a.size() < 200);                      // 200 sent, some lost, some duplicated: the faults did act
}

static void test_bytes_are_never_modified() {                         // opaque frames
    Link l;
    Bytes original(3000);
    std::uint32_t x = 12345;
    for (auto& b : original) { x = x * 1664525u + 1013904223u; b = static_cast<std::uint8_t>(x >> 24); }
    KRITVA_CHECK(l.t.nexus().send(original).has_value());
    original[0] ^= 0xFF;                                                 // the caller's buffer can change afterwards: it was copied
    const auto got = l.t.edge().receive();
    KRITVA_CHECK(got && got->size() == 3000 && (*got)[0] == static_cast<std::uint8_t>(original[0] ^ 0xFF));
    original[0] ^= 0xFF;
    KRITVA_CHECK(*got == original);
}

int main() {
    test_initial_state_and_clock();
    test_time_never_moves_by_itself();
    test_zero_latency();
    test_fixed_latency_boundaries();
    test_step_jumps_to_the_next_due_time();
    test_ordering_and_equal_due_times();
    test_explicit_drop_and_stats();
    test_explicit_reorder();
    test_explicit_duplicate();
    test_fault_on_drop_wins_over_duplicate();
    test_disconnect_and_reconnect();
    test_disconnect_is_connectivity_only();
    test_frame_size_bounds();
    test_refused_sends_consume_no_fault_index();
    test_queue_capacity();
    test_configure_validation();
    test_extreme_delays_saturate();
    test_probabilistic_extremes();
    test_probability_boundaries_and_reorder_delay();
    test_configure_restarts_the_send_index();
    test_repeatability_and_seed_sensitivity();
    test_bytes_are_never_modified();
    std::printf("simulated_transport_test: PASS\n");
    return 0;
}
