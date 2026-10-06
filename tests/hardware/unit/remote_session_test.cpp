//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_session_test.cpp
// Description : Unit tests of the Nexus-side session: handshake, correlation, deadlines, response admission by
//               correlation (never by the sequence watermark), notices, link loss and retransmission.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: RR-002; PR-003; FRL-002 (mechanism); NDR-003
// API         : REMOTE-SESSION-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include "../support/nexus_rig.hpp"
#include "../support/test_endpoints.hpp"

using namespace kritva::hardware::remote;
using namespace kritva::hardware::remote::test;
using kritva::core::ErrorCode;
using kritva::core::LifecycleState;
using kritva::hardware::DeviceId;
using kritva::hardware::EndpointId;

static const EndpointAddress kMotorCommand{NodeId{kEdgeNode}, DeviceId{kMotor}, EndpointId{kCommand}};
static const EndpointAddress kAccelerometer{NodeId{kEdgeNode}, DeviceId{kImu}, EndpointId{kAccel}};

static constexpr std::uint64_t kMs = 1'000'000;

static ErrorCode code_of_observe(NexusRig& rig, const EndpointAddress& a = kMotorCommand) {
    const auto r = rig.session().observe(a);
    return r.has_value() ? ErrorCode::NONE : r.error().code;
}

static void test_open_establishes_the_session() {                          // PR-001, NDR-003
    NexusRig rig;
    KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED && !rig.session().session().valid());
    KRITVA_CHECK(rig.node.connect().has_value());
    KRITVA_CHECK(rig.session().state() == SessionState::CONNECTED && rig.session().session().value() == 1);
    KRITVA_CHECK(rig.edge().session().value() == 1 && rig.edge().sealed());
    KRITVA_CHECK(rig.session().discovery().devices.size() == 2 && rig.session().stats().requests_sent == 2 && rig.session().stats().responses_accepted == 2);
    KRITVA_CHECK(rig.session().last_valid_frame_ns() == rig.link().now_ns());
    KRITVA_CHECK(rig.node.connect().error().code == ErrorCode::INVALID_STATE);        // once
    KRITVA_CHECK(rig.session().open().error().code == ErrorCode::INVALID_STATE);       // the topology is registered: use reopen()
}

static void test_calls_need_a_connected_session() {
    NexusRig rig;
    const auto sent = [&] { return rig.session().stats().requests_sent; };
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::RESOURCE_UNAVAILABLE && sent() == 0);     // never opened: nothing is sent
    KRITVA_CHECK(rig.session().write(kMotorCommand, 0.1).error().code == ErrorCode::RESOURCE_UNAVAILABLE);
    KRITVA_CHECK(rig.session().reopen().error().code == ErrorCode::INVALID_STATE);            // never opened
    KRITVA_CHECK(rig.node.connect().has_value());
    const auto before = sent();
    rig.session().close();
    KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED && !rig.session().session().valid());
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::RESOURCE_UNAVAILABLE && sent() == before);
}

static std::unique_ptr<RemoteSession> make_session(EdgeRig& rig, std::uint64_t nexus, std::uint64_t edge, LinkTiming timing, bool tick = true) {
    RemoteSessionConfig c;
    c.nexus_node = NodeId{nexus};
    c.edge_node = NodeId{edge};
    c.timing = timing;
    if (tick) c.peer_tick = [&rig] { rig.edge->poll(); };
    return std::make_unique<RemoteSession>(rig.link.nexus(), std::move(c));
}

static void test_handshake_failures_end_disconnected() {
    {   // the Edge is not the one expected
        EdgeRig rig;
        auto s = make_session(rig, kNexusNode, kEdgeNode + 1, {});
        const auto r = s->open();
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT && s->state() == SessionState::DISCONNECTED && !s->session().valid());
    }
    {   // invalid configuration: nothing is sent
        EdgeRig rig;
        LinkTiming bad;
        bad.heartbeat_timeout_ms = 10;
        auto s = make_session(rig, kNexusNode, kEdgeNode, bad);
        KRITVA_CHECK(s->open().error().code == ErrorCode::INVALID_ARGUMENT && s->stats().requests_sent == 0);
        auto s2 = make_session(rig, 0, kEdgeNode, {});
        KRITVA_CHECK(s2->open().error().code == ErrorCode::INVALID_ARGUMENT && s2->stats().requests_sent == 0);
    }
    {   // an unsupported Edge topology
        EdgeRig rig(false);
        kritva::hardware::Device odd(kritva::hardware::DeviceInfo::create(kritva::hardware::DeviceId{5}, "odd").value());
        KRITVA_CHECK(odd.add_endpoint(std::make_unique<kritva::hardware::test::TestSensor>(0x1001, "fake")).has_value() && rig.registry.register_device(odd).has_value());
        auto s = make_session(rig, kNexusNode, kEdgeNode, {});
        const auto r = s->open();
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::UNSUPPORTED && s->state() == SessionState::DISCONNECTED);
    }
    {   // nobody answers: the deadline, exactly
        EdgeRig rig;
        auto s = make_session(rig, kNexusNode, kEdgeNode, {}, false);
        const auto t0 = rig.link.now_ns();
        const auto r = s->open();
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::TIMEOUT && s->state() == SessionState::DISCONNECTED);
        KRITVA_CHECK(rig.link.now_ns() - t0 == 100 * kMs && s->stats().timeouts == 1);
    }
    {   // the discovery answer is lost
        EdgeRig rig;
        DirectionConfig lossy;
        lossy.faults[1].drop = true;                                         // answer 0 is the HELLO_ACK, 1 the discovery
        KRITVA_CHECK(rig.link.configure(LinkDirection::EDGE_TO_NEXUS, lossy).has_value());
        auto s = make_session(rig, kNexusNode, kEdgeNode, {});
        KRITVA_CHECK(s->open().error().code == ErrorCode::TIMEOUT && s->state() == SessionState::DISCONNECTED && !s->session().valid());
    }
}

static void test_a_fresh_reopen_replaces_the_session_and_compares_topology() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    KRITVA_CHECK(rig.session().reopen().has_value());
    KRITVA_CHECK(rig.session().session().value() == 2 && rig.edge().session().value() == 2 && rig.session().state() == SessionState::CONNECTED);   // NDR-003
    KRITVA_CHECK(rig.session().stats().requests_sent == 4);
    // The Nexus counters restart with the session: the next request is sequence 3 (HELLO 1, DISCOVERY 2).
    KRITVA_CHECK(rig.session().next_request_sequence() == 3);
}

// ---- admission by correlation ------------------------------------------------------------------------------------

static void test_deadline_is_inclusive_and_exact() {                       // RR-002
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    DirectionConfig exactly;
    exactly.latency_ns = 100 * kMs;                                          // the answer is due exactly at the deadline
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, exactly).has_value());
    auto t0 = rig.link().now_ns();
    KRITVA_CHECK(rig.session().observe(kMotorCommand).has_value());         // accepted
    KRITVA_CHECK(rig.link().now_ns() - t0 == 100 * kMs && rig.session().stats().timeouts == 0);

    DirectionConfig late;
    late.latency_ns = 100 * kMs + 1;                                         // one nanosecond too late
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, late).has_value());
    t0 = rig.link().now_ns();
    const auto r = rig.session().observe(kMotorCommand);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::TIMEOUT);
    KRITVA_CHECK(rig.link().now_ns() - t0 == 100 * kMs && rig.session().stats().timeouts == 1);   // virtual time stops at the deadline
    KRITVA_CHECK(rig.session().state() == SessionState::CONNECTED);         // a request timeout does not end the session
}

static void test_a_late_response_is_counted_and_never_satisfies_another_request() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    DirectionConfig slow;
    slow.latency_ns = 150 * kMs;
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, slow).has_value());
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::TIMEOUT);
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, DirectionConfig{}).has_value());
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE);                  // the next request is answered by its own response
    KRITVA_CHECK(rig.session().stats().late_responses == 0);                // the late one has not arrived yet
    KRITVA_CHECK(rig.link().advance(100 * kMs).has_value());
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE);
    KRITVA_CHECK(rig.session().stats().late_responses == 1 && rig.session().stats().unknown_correlation == 0);
}

static void test_a_duplicate_response_is_dropped() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    DirectionConfig dup;
    dup.faults[0].duplicate = true;
    dup.faults[0].duplicate_extra_delay_ns = 5 * kMs;
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, dup).has_value());
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE);                  // answered by the first copy
    KRITVA_CHECK(rig.session().stats().unknown_correlation == 0);
    KRITVA_CHECK(rig.link().advance(5 * kMs).has_value());                  // the copy of the first answer becomes due
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE);                  // and arrives during the next call
    KRITVA_CHECK(rig.session().stats().unknown_correlation == 1 && rig.session().stats().late_responses == 0);
    KRITVA_CHECK(rig.session().stats().responses_accepted == 4);            // HELLO_ACK, discovery, two observes
}

static void test_a_duplicate_response_arriving_at_the_same_instant_is_dropped() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    DirectionConfig dup;
    dup.faults[0].duplicate = true;                                          // the copy is due at the same time as the original
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, dup).has_value());
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE);
    KRITVA_CHECK(rig.session().stats().unknown_correlation == 1 && rig.session().stats().responses_accepted == 3);   // taken once, the copy dropped
}

// A response that arrives after a later heartbeat is accepted: responses are never compared with the notice watermark.
static void test_a_response_after_a_later_heartbeat_is_accepted() {         // the I4-001 correction
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    const std::uint64_t sid = rig.session().session().value();
    inject_to_nexus(rig.link(), MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1})), 1000, sid, 0);   // sequence 1000 arrives first
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE);                  // the response (a low Edge sequence) is still admitted
    KRITVA_CHECK(rig.session().stats().notices == 1 && rig.session().stats().stale_notices == 0);
    // And the watermark applies to notices only: a notice below it is stale, one above is not.
    inject_to_nexus(rig.link(), MessageType::HEARTBEAT, must(encode(HeartbeatPayload{2})), 999, sid, 0);
    inject_to_nexus(rig.link(), MessageType::HEARTBEAT, must(encode(HeartbeatPayload{3})), 1000, sid, 0);
    inject_to_nexus(rig.link(), MessageType::HEARTBEAT, must(encode(HeartbeatPayload{4})), 1001, sid, 0);
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE);
    KRITVA_CHECK(rig.session().stats().notices == 2 && rig.session().stats().stale_notices == 2);
}

struct InjectOnce {
    NexusRig& rig;
    std::function<void()> action;
    bool done{false};
    explicit InjectOnce(NexusRig& r, std::function<void()> a) : rig(r), action(std::move(a)) {
        rig.before_edge = [this] { if (!done) { done = true; action(); } };
    }
    ~InjectOnce() { rig.before_edge = nullptr; }
};

static void test_unexpected_responses_are_dropped_and_the_real_one_is_taken() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    const std::uint64_t sid = rig.session().session().value();
    const Bytes ok_observe = must(encode(ObserveResponsePayload{}));
    const auto stats = [&]() -> const RemoteStats& { return rig.session().stats(); };
    {   // another session
        const std::uint64_t seq = rig.session().next_request_sequence();
        InjectOnce once(rig, [&] { inject_to_nexus(rig.link(), MessageType::OBSERVE_RESPONSE, ok_observe, 500, sid + 1, seq); });
        KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE && stats().wrong_session == 1);
    }
    {   // an unknown correlation
        const std::uint64_t seq = rig.session().next_request_sequence();
        InjectOnce once(rig, [&] { inject_to_nexus(rig.link(), MessageType::OBSERVE_RESPONSE, ok_observe, 501, sid, seq + 100); });
        KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE && stats().unknown_correlation == 1);
    }
    {   // the right correlation, the wrong type
        const std::uint64_t seq = rig.session().next_request_sequence();
        InjectOnce once(rig, [&] { inject_to_nexus(rig.link(), MessageType::WRITE_RESPONSE, must(encode(StatusPayload{})), 502, sid, seq); });
        KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE && stats().wrong_type == 1);
    }
    {   // a request type or a response of a type that cannot reach a Nexus is a frame error
        const std::uint64_t seq = rig.session().next_request_sequence();
        InjectOnce once(rig, [&] { inject_to_nexus(rig.link(), MessageType::READ_REQUEST, addr(kImu, kAccel), 503, sid, 0); (void)seq; });
        KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE && stats().frame_errors == 1);
    }
    KRITVA_CHECK(stats().responses_accepted == 6);                           // HELLO_ACK, discovery and four real answers
}

static void test_a_malformed_matching_response_fails_the_request_at_once() {   // decision 9
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    const std::uint64_t sid = rig.session().session().value();
    const std::uint64_t seq = rig.session().next_request_sequence();
    const auto t0 = rig.link().now_ns();
    {
        InjectOnce once(rig, [&] { inject_to_nexus(rig.link(), MessageType::OBSERVE_RESPONSE, Bytes(3, 0), 600, sid, seq); });
        const auto r = rig.session().observe(kMotorCommand);
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
    }
    KRITVA_CHECK(rig.link().now_ns() == t0);                                  // it did not wait for the deadline
    KRITVA_CHECK(rig.session().stats().malformed_responses == 1 && rig.session().stats().timeouts == 0);
    // The Edge's own (good) answer to that request arrives later: the request is over, so it is dropped as unknown.
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE);
    KRITVA_CHECK(rig.session().stats().unknown_correlation == 1);
}

static void test_a_matching_protocol_error_fails_the_request_at_once() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    const std::uint64_t sid = rig.session().session().value();
    const std::uint64_t seq = rig.session().next_request_sequence();
    const auto t0 = rig.link().now_ns();
    InjectOnce once(rig, [&] {
        inject_to_nexus(rig.link(), MessageType::PROTOCOL_ERROR, must(encode_protocol_error(StatusPayload{{ErrorCode::INVALID_ARGUMENT, "refused"}})), 601, sid, seq);
    });
    const auto r = rig.session().observe(kMotorCommand);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT && r.error().message == "refused" && rig.link().now_ns() == t0);
    // A PROTOCOL_ERROR that matches nothing is just a dropped response.
    inject_to_nexus(rig.link(), MessageType::PROTOCOL_ERROR, must(encode_protocol_error(StatusPayload{{ErrorCode::INVALID_ARGUMENT, "x"}})), 602, sid, 999);
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE && rig.session().stats().unknown_correlation >= 1);
}

static void test_notices_are_admitted_per_session() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    const std::uint64_t sid = rig.session().session().value();
    const auto hb = must(encode(HeartbeatPayload{9}));
    inject_to_nexus(rig.link(), MessageType::HEARTBEAT, hb, 10, sid + 1, 0);               // another session
    inject_to_nexus(rig.link(), MessageType::HEARTBEAT, hb, 10, 0, 0);                     // no session
    rig.link().edge().send(Bytes(12, 0xAB));                                              // garbage
    inject_to_nexus(rig.link(), MessageType::HEARTBEAT, hb, 10, sid, 0);
    inject_to_nexus(rig.link(), MessageType::HEARTBEAT, hb, 10, sid, 0);                   // a repeat
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE);
    const auto& st = rig.session().stats();
    KRITVA_CHECK(st.wrong_session == 2 && st.frame_errors == 1 && st.notices == 1 && st.stale_notices == 1);
    KRITVA_CHECK(rig.session().reopen().has_value());                                      // a new session starts from nothing
    inject_to_nexus(rig.link(), MessageType::HEARTBEAT, hb, 10, rig.session().session().value(), 0);
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE && rig.session().stats().notices == 2);
}

// ---- link loss ----------------------------------------------------------------------------------------------------

static void test_link_loss_during_a_call_and_explicit_recovery() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    {
        InjectOnce once(rig, [&] { rig.link().disconnect(); });
        const auto r = rig.session().observe(kMotorCommand);
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::RESOURCE_UNAVAILABLE && r.error().message == "the link is down");
    }
    KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED && !rig.session().session().valid());
    const auto sent = rig.session().stats().requests_sent;
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::RESOURCE_UNAVAILABLE && rig.session().stats().requests_sent == sent);   // nothing is sent any more
    KRITVA_CHECK(rig.session().stats().timeouts == 0);                                                                    // not a timeout
    // No automatic reconnect: only an explicit reopen() brings the link and a fresh session back.
    KRITVA_CHECK(rig.link().link_state() == LinkState::DISCONNECTED);
    KRITVA_CHECK(rig.session().reopen().has_value() && rig.session().state() == SessionState::CONNECTED && rig.session().session().value() == 2);
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE);
}

static void test_a_down_link_refuses_the_first_send() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    rig.link().disconnect();
    const auto r = rig.session().observe(kMotorCommand);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::RESOURCE_UNAVAILABLE && rig.session().state() == SessionState::DISCONNECTED);
}


// A HELLO_ACK has to match the HELLO and the Edge that was expected, field by field.
static void test_an_inconsistent_hello_ack_is_refused() {
    struct Case { const char* what; std::uint64_t header_session, node, session; std::uint16_t major, minor; std::uint32_t period, timeout; };
    const Case cases[] = {
        {"another edge node", 9, kEdgeNode + 1, 9, 1, 0, 100, 300},
        {"header session differs from the payload session", 9, kEdgeNode, 8, 1, 0, 100, 300},
        {"major differs", 9, kEdgeNode, 9, 2, 0, 100, 300},
        {"minor differs", 9, kEdgeNode, 9, 1, 1, 100, 300},
        {"heartbeat period differs", 9, kEdgeNode, 9, 1, 0, 101, 300},
        {"heartbeat timeout differs", 9, kEdgeNode, 9, 1, 0, 100, 301},
    };
    for (const Case& c : cases) {
        EdgeRig rig;
        RemoteSessionConfig cfg;
        cfg.nexus_node = NodeId{kNexusNode};
        cfg.edge_node = NodeId{kEdgeNode};
        bool injected = false;
        cfg.peer_tick = [&] {
            if (injected) return;
            injected = true;
            HelloAckPayload ack;
            ack.node_id = c.node;
            ack.session_id = c.session;
            ack.major = c.major;
            ack.minor = c.minor;
            ack.heartbeat_period_ms = c.period;
            ack.heartbeat_timeout_ms = c.timeout;
            inject_to_nexus(rig.link, MessageType::HELLO_ACK, must(encode(ack)), 1, c.header_session, 1);
        };
        RemoteSession s(rig.link.nexus(), std::move(cfg));
        const auto r = s.open();
        if (r.has_value() || r.error().code != ErrorCode::INVALID_ARGUMENT || s.state() != SessionState::DISCONNECTED || s.session().valid()) {
            std::fprintf(stderr, "not refused: %s\n", c.what);
            std::exit(1);
        }
    }
}

static void test_the_states_of_a_reopen_follow_the_handshake() {            // protocol section 9
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    std::vector<std::pair<SessionState, bool>> seen;                          // (state, session valid) at each pump step
    rig.before_edge = [&] { seen.push_back({rig.session().state(), rig.session().session().valid()}); };
    KRITVA_CHECK(rig.session().reopen().has_value());
    rig.before_edge = nullptr;
    KRITVA_CHECK(seen.size() == 2);                                           // one step for HELLO, one for the discovery
    KRITVA_CHECK(seen[0].first == SessionState::CONNECTING && !seen[0].second);  // the old session is gone, the new one not yet known
    KRITVA_CHECK(seen[1].first == SessionState::NEGOTIATING && seen[1].second);
    KRITVA_CHECK(rig.session().state() == SessionState::CONNECTED);
}

static void test_the_pump_never_runs_past_the_deadline_whatever_the_quantum() {
    LinkTiming timing;
    timing.pump_quantum_ms = 7;                                               // 100 is not a multiple of 7
    NexusRig rig(timing);
    KRITVA_CHECK(rig.node.connect().has_value());
    DirectionConfig slow;
    slow.latency_ns = 500 * kMs;
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, slow).has_value());
    const auto t0 = rig.link().now_ns();
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::TIMEOUT);
    KRITVA_CHECK(rig.link().now_ns() - t0 == 100 * kMs);
}

static void test_a_response_refreshes_the_liveness_time() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    DirectionConfig c;
    c.latency_ns = 3 * kMs;
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, c).has_value());
    const auto before = rig.session().last_valid_frame_ns();
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE);
    KRITVA_CHECK(rig.session().last_valid_frame_ns() > before && rig.session().last_valid_frame_ns() == rig.link().now_ns());
}

// After a successful retransmission the first attempt's own (late) answer is a duplicate of an answered request, not a late one.
static void test_the_late_answer_of_a_retransmitted_write_is_a_duplicate() {
    NexusRig rig;
    rig.connect_and_run();
    DirectionConfig slow;
    slow.faults[0].extra_delay_ns = 150 * kMs;                               // the answer to the first attempt arrives after its deadline
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, slow).has_value());
    KRITVA_CHECK(rig.session().write(kMotorCommand, 0.5).error().code == ErrorCode::TIMEOUT);
    KRITVA_CHECK(rig.session().retransmit_write(kMotorCommand).has_value());  // answered from the Edge's ledger
    KRITVA_CHECK(rig.link().advance(100 * kMs).has_value());
    KRITVA_CHECK(code_of_observe(rig) == ErrorCode::NONE);                    // the first attempt's answer arrives during this call
    KRITVA_CHECK(rig.session().stats().late_responses == 0 && rig.session().stats().unknown_correlation == 1);
}

// The Nexus does not trust the Edge to have refused an unsupported topology: it checks the typed mapping itself.
static void test_the_nexus_checks_the_typed_mapping_of_a_discovery() {
    EdgeRig rig;
    RemoteSessionConfig cfg;
    cfg.nexus_node = NodeId{kNexusNode};
    cfg.edge_node = NodeId{kEdgeNode};
    int tick = 0;
    cfg.peer_tick = [&] {
        ++tick;
        if (tick == 2) {                                                        // the discovery step: a crafted answer arrives before the Edge's own
            DiscoveryResponsePayload bad;
            bad.devices = {DiscoveredDevice{1, "imu", {DiscoveredEndpoint{1, "odd", WireDirection::SENSOR, {DiscoveredCapability{0x1004, "unknown"}}}}}};
            inject_to_nexus(rig.link, MessageType::DISCOVERY_RESPONSE, must(encode(bad)), 50, rig.edge->session().value(), 2);
        }
        rig.edge->poll();
    };
    RemoteSession s(rig.link.nexus(), std::move(cfg));
    const auto r = s.open();
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::UNSUPPORTED && s.state() == SessionState::DISCONNECTED);
}

// ---- retransmission -------------------------------------------------------------------------------------------------

static void test_retransmission_is_explicit_and_uses_the_same_sequence() {   // at-most-once at the Edge
    NexusRig rig;
    rig.connect_and_run();                                                    // every endpoint RUNNING on a fresh session
    DirectionConfig lossy;
    lossy.faults[0].drop = true;                                              // the answer to the next request is lost
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, lossy).has_value());
    KRITVA_CHECK(!rig.session().last_write_sequence(kMotorCommand).has_value());
    const auto t0 = rig.link().now_ns();
    const auto r = rig.session().write(kMotorCommand, 0.5);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::TIMEOUT && rig.link().now_ns() - t0 == 100 * kMs);
    KRITVA_CHECK(rig.motor().model().velocity_rad_s == 0.5 && rig.edge().stats().writes_applied == 1);          // applied, answer lost: the outcome is UNKNOWN to the Nexus
    const std::uint64_t seq = *rig.session().last_write_sequence(kMotorCommand);
    const std::uint64_t next_before = rig.session().next_request_sequence();   // heartbeats use sequence numbers too, so compare with this, not with seq + 1
    KRITVA_CHECK(next_before > seq);                                         // the Nexus did not retransmit by itself
    const std::uint64_t ops = rig.motor().command().statistics().sample_count.value();

    const auto again = rig.session().retransmit_write(kMotorCommand);
    KRITVA_CHECK(again.has_value() && again.value().status.ok());
    KRITVA_CHECK(rig.edge().stats().writes_resent == 1 && rig.edge().stats().writes_applied == 1 && rig.motor().command().statistics().sample_count.value() == ops);
    KRITVA_CHECK(rig.session().next_request_sequence() == next_before && rig.session().stats().retransmissions == 1);   // the same sequence was reused: nothing new was numbered
    // A new write is a new request.
    KRITVA_CHECK(rig.session().write(kMotorCommand, 0.25).has_value() && *rig.session().last_write_sequence(kMotorCommand) > seq);
}

static void test_retransmission_preconditions() {
    NexusRig rig;
    rig.connect_and_run();
    KRITVA_CHECK(rig.session().retransmit_write(kMotorCommand).error().code == ErrorCode::INVALID_STATE);   // there is nothing to retransmit
    KRITVA_CHECK(rig.session().write(kMotorCommand, 0.5).has_value());
    KRITVA_CHECK(rig.session().retransmit_write(kAccelerometer).error().code == ErrorCode::INVALID_STATE);  // another endpoint's write
    KRITVA_CHECK(rig.session().reopen().has_value());                                                        // a new session: the sequence is no longer meaningful
    KRITVA_CHECK(rig.session().retransmit_write(kMotorCommand).error().code == ErrorCode::INVALID_STATE && !rig.session().last_write_sequence(kMotorCommand));
    rig.session().close();
    KRITVA_CHECK(rig.session().retransmit_write(kMotorCommand).error().code == ErrorCode::RESOURCE_UNAVAILABLE);
}

// ---- determinism -----------------------------------------------------------------------------------------------------

static std::vector<std::uint64_t> scripted_run(std::uint64_t seed) {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    DirectionConfig c;
    c.latency_ns = 2 * kMs;
    c.seed = seed;
    c.drop_permille = 150;
    c.duplicate_permille = 150;
    c.reorder_permille = 200;
    c.reorder_delay_ns = 7 * kMs;
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, c).has_value() && rig.link().configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    std::vector<std::uint64_t> log;
    for (int i = 0; i < 30; ++i) {
        const auto r = rig.session().observe(kMotorCommand);
        log.push_back(r.has_value() ? 0 : static_cast<std::uint64_t>(r.error().code));
        log.push_back(rig.link().now_ns());
        if (rig.session().state() != SessionState::CONNECTED) break;
    }
    const auto& s = rig.session().stats();
    for (const auto v : {s.requests_sent, s.responses_accepted, s.timeouts, s.late_responses, s.unknown_correlation, s.wrong_session}) log.push_back(v);
    return log;
}

static void test_repeatability() {
    const auto a = scripted_run(5);
    KRITVA_CHECK(a == scripted_run(5) && a != scripted_run(6));
    KRITVA_CHECK(a.size() > 10);
}

int main() {
    test_open_establishes_the_session();
    test_an_inconsistent_hello_ack_is_refused();
    test_the_states_of_a_reopen_follow_the_handshake();
    test_the_nexus_checks_the_typed_mapping_of_a_discovery();
    test_the_pump_never_runs_past_the_deadline_whatever_the_quantum();
    test_a_response_refreshes_the_liveness_time();
    test_calls_need_a_connected_session();
    test_handshake_failures_end_disconnected();
    test_a_fresh_reopen_replaces_the_session_and_compares_topology();
    test_deadline_is_inclusive_and_exact();
    test_a_late_response_is_counted_and_never_satisfies_another_request();
    test_a_duplicate_response_is_dropped();
    test_a_duplicate_response_arriving_at_the_same_instant_is_dropped();
    test_a_response_after_a_later_heartbeat_is_accepted();
    test_unexpected_responses_are_dropped_and_the_real_one_is_taken();
    test_a_malformed_matching_response_fails_the_request_at_once();
    test_a_matching_protocol_error_fails_the_request_at_once();
    test_notices_are_admitted_per_session();
    test_link_loss_during_a_call_and_explicit_recovery();
    test_a_down_link_refuses_the_first_send();
    test_retransmission_is_explicit_and_uses_the_same_sequence();
    test_the_late_answer_of_a_retransmitted_write_is_a_duplicate();
    test_retransmission_preconditions();
    test_repeatability();
    std::printf("remote_session_test: PASS\n");
    return 0;
}
