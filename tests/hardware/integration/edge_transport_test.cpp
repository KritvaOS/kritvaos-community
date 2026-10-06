//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : edge_transport_test.cpp
// Description : Integration of the EdgeHost with the simulated transport: latency, link duplicates, reordering,
//               loss and disconnect, all through real frames.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: ER-001; ER-003; SR-002 (mechanism); TR-002 (HUMAN SAFETY REVIEW OPEN)
// API         : EDGE-TRANSPORT-IT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include "../support/edge_rig.hpp"

using namespace kritva::hardware::remote;
using namespace kritva::hardware::remote::test;
using kritva::core::ErrorCode;
using kritva::core::LifecycleState;

static Bytes write_payload(double v) {
    return must(encode(WriteRequestPayload{AddressPayload{kEdgeNode, kMotor, kCommand}, v}));
}

// Advance virtual time to the next frame in flight, let the Edge handle what is due, and collect its replies.
static std::vector<Reply> run_until_idle(EdgeRig& rig) {
    std::vector<Reply> out;
    while (rig.link.step()) {
        rig.edge->poll();
        for (auto& r : rig.drain()) out.push_back(std::move(r));
    }
    return out;
}

static void test_latency_and_virtual_time() {
    EdgeRig rig;
    DirectionConfig c;
    c.latency_ns = 1000;
    KRITVA_CHECK(rig.link.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value() && rig.link.configure(LinkDirection::EDGE_TO_NEXUS, c).has_value());
    rig.seq = 1;
    rig.send_raw(EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 1, 0, 100, 300})), 1, 0));
    KRITVA_CHECK(rig.edge->poll() == 0 && !rig.edge->session().valid());                  // not due yet
    KRITVA_CHECK(rig.link.advance(999).has_value() && rig.edge->poll() == 0);
    KRITVA_CHECK(rig.link.advance(1).has_value() && rig.edge->poll() == 1 && rig.edge->session().value() == 1);   // due exactly now
    KRITVA_CHECK(rig.edge->last_valid_frame_ns() == 1000);
    KRITVA_CHECK(rig.drain().empty());                                                    // the answer is in flight for another 1000 ns
    KRITVA_CHECK(rig.link.advance(1000).has_value());
    const auto replies = rig.drain();
    KRITVA_CHECK(replies.size() == 1 && replies[0].header.type == MessageType::HELLO_ACK && replies[0].header.session_id == 1);
}

static void test_a_link_duplicated_write_is_applied_once() {                           // SR-002 over the real transport
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    DirectionConfig c;
    c.latency_ns = 100;
    c.faults[0].duplicate = true;
    c.faults[0].duplicate_extra_delay_ns = 5000;
    KRITVA_CHECK(rig.link.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    const std::uint64_t ops = rig.motor.command().statistics().sample_count.value();
    const std::uint64_t s = rig.send(MessageType::WRITE_REQUEST, write_payload(0.5));
    const auto replies = run_until_idle(rig);
    KRITVA_CHECK(replies.size() == 2 && replies[0].header.correlation_id == s && replies[1].header.correlation_id == s);
    KRITVA_CHECK(replies[0].payload == replies[1].payload);                                // the cached response again
    KRITVA_CHECK(rig.motor.command().statistics().sample_count.value() == ops + 1 && rig.stats().writes_applied == 1 && rig.stats().writes_resent == 1);
}

static void test_a_reordered_write_is_never_applied_late() {                           // SR-002
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    DirectionConfig c;
    c.latency_ns = 100;
    c.faults[0].extra_delay_ns = 5000;                                                     // the first write is held back past the second
    KRITVA_CHECK(rig.link.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    const std::uint64_t first = rig.send(MessageType::WRITE_REQUEST, write_payload(0.9));
    const std::uint64_t second = rig.send(MessageType::WRITE_REQUEST, write_payload(0.1));
    const auto replies = run_until_idle(rig);
    KRITVA_CHECK(replies.size() == 1 && replies[0].header.correlation_id == second);       // the newer command was applied and answered
    KRITVA_CHECK(rig.motor.model().velocity_rad_s == 0.1 && rig.stats().writes_applied == 1 && rig.stats().stale_frames == 1);
    (void)first;                                                                           // the older command arrived late and was dropped
}

static void test_a_lost_response_does_not_apply_the_command_twice() {                  // the at-most-once rule; the outcome is unknown to the Nexus
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    DirectionConfig lossy;
    lossy.faults[0].drop = true;                                                           // the Edge's first answer is lost
    KRITVA_CHECK(rig.link.configure(LinkDirection::EDGE_TO_NEXUS, lossy).has_value());
    const std::uint64_t s = rig.send(MessageType::WRITE_REQUEST, write_payload(0.5));
    KRITVA_CHECK(run_until_idle(rig).empty());                                             // the Nexus sees nothing: the outcome is unknown to it
    KRITVA_CHECK(rig.motor.model().velocity_rad_s == 0.5 && rig.stats().writes_applied == 1);
    // A retransmission of the SAME frame (a caller's decision, never automatic) is answered from the ledger, not applied again.
    const std::uint64_t ops = rig.motor.command().statistics().sample_count.value();
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.5), s, rig.session));
    const auto replies = run_until_idle(rig);
    KRITVA_CHECK(replies.size() == 1 && replies[0].header.correlation_id == s && decode<StatusPayload>(replies[0].payload).value.status.ok());
    KRITVA_CHECK(rig.motor.command().statistics().sample_count.value() == ops && rig.stats().writes_resent == 1);
}

static void test_a_link_duplicated_hello_replaces_the_session() {
    EdgeRig rig;
    DirectionConfig c;
    c.faults[0].duplicate = true;
    c.faults[0].duplicate_extra_delay_ns = 10;
    KRITVA_CHECK(rig.link.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    rig.send_raw(EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 1, 0, 100, 300})), 1, 0));
    const auto replies = run_until_idle(rig);
    KRITVA_CHECK(replies.size() == 2 && replies[0].header.session_id == 1 && replies[1].header.session_id == 2);   // two sessions: HELLO is not idempotent
    KRITVA_CHECK(rig.edge->session().value() == 2 && rig.stats().hellos_accepted == 2);
}

static void test_link_loss_is_connectivity_only_for_the_edge() {                       // no safety policy in I4-004: that is I4-006
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
    rig.link.disconnect();
    KRITVA_CHECK(rig.link.advance(10'000'000'000ull).has_value());                        // ten virtual seconds
    KRITVA_CHECK(rig.edge->poll() == 0);
    KRITVA_CHECK(rig.motor.command().lifecycle_state() == LifecycleState::RUNNING && rig.motor.command().effective_velocity() == 0.5);
    KRITVA_CHECK(rig.edge->session().value() == 1 && rig.stats().actuators_stopped_on_new_session == 0);
    // After the link is back the session and its ledger are exactly as they were: the last write can still be resent, not re-applied.
    KRITVA_CHECK(rig.link.connect().has_value());
    const std::uint64_t ops = rig.motor.command().statistics().sample_count.value();
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.5), rig.seq, rig.session));
    KRITVA_CHECK(rig.poll().size() == 1 && rig.stats().writes_resent == 1 && rig.motor.command().statistics().sample_count.value() == ops);
}

static void test_an_answer_the_link_refuses_is_counted() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    DirectionConfig tiny;
    tiny.queue_capacity = 1;
    KRITVA_CHECK(rig.link.configure(LinkDirection::EDGE_TO_NEXUS, tiny).has_value());
    rig.send(MessageType::DISCOVERY_REQUEST, {});
    rig.send(MessageType::DISCOVERY_REQUEST, {});                                          // two answers, room for one
    KRITVA_CHECK(rig.edge->poll() == 2 && rig.stats().send_failures == 1 && rig.stats().requests_served == 2);
}

// The same scripted traffic over the same link configuration gives byte-identical Edge output.
static std::vector<Bytes> scripted_run(std::uint64_t seed) {
    EdgeRig rig;
    DirectionConfig c;
    c.latency_ns = 300;
    c.seed = seed;
    c.drop_permille = 100;
    c.duplicate_permille = 150;
    c.reorder_permille = 250;
    c.reorder_delay_ns = 2000;
    KRITVA_CHECK(rig.link.configure(LinkDirection::NEXUS_TO_EDGE, c).has_value() && rig.link.configure(LinkDirection::EDGE_TO_NEXUS, c).has_value());
    rig.seq = 1;
    rig.send_raw(EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 1, 0, 100, 300})), 1, 0));
    std::vector<Bytes> out;
    for (auto& r : run_until_idle(rig)) out.push_back(EdgeRig::frame(r.header.type, r.payload, r.header.sequence, r.header.session_id, r.header.correlation_id));
    rig.session = rig.edge->session().value();
    if (rig.session == 0) return out;
    for (int i = 0; i < 40; ++i) {
        rig.send(i % 4 == 0 ? MessageType::DISCOVERY_REQUEST : MessageType::OBSERVE_REQUEST, i % 4 == 0 ? Bytes{} : addr(kImu, kAccel));
        rig.link.advance(150);
        rig.edge->poll();
        for (auto& r : rig.drain()) out.push_back(EdgeRig::frame(r.header.type, r.payload, r.header.sequence, r.header.session_id, r.header.correlation_id));
    }
    for (auto& r : run_until_idle(rig)) out.push_back(EdgeRig::frame(r.header.type, r.payload, r.header.sequence, r.header.session_id, r.header.correlation_id));
    return out;
}

static void test_repeatability() {
    const auto a = scripted_run(11);
    const auto b = scripted_run(11);
    KRITVA_CHECK(a.size() > 5 && a == b);
    KRITVA_CHECK(scripted_run(12) != a);
}

int main() {
    test_latency_and_virtual_time();
    test_a_link_duplicated_write_is_applied_once();
    test_a_reordered_write_is_never_applied_late();
    test_a_lost_response_does_not_apply_the_command_twice();
    test_a_link_duplicated_hello_replaces_the_session();
    test_link_loss_is_connectivity_only_for_the_edge();
    test_an_answer_the_link_refuses_is_counted();
    test_repeatability();
    std::printf("edge_transport_test: PASS\n");
    return 0;
}
