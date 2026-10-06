//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : edge_session_test.cpp
// Description : Unit tests of the EdgeHost session layer: intake, HELLO, session replacement, the request
//               watermark, malformed traffic, heartbeats and discovery.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: ER-001; ER-002; ER-003; PR-001; PR-003
// API         : EDGE-SESSION-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include "../support/edge_rig.hpp"
#include "../support/test_endpoints.hpp"

using namespace kritva::hardware::remote;
using namespace kritva::hardware::remote::test;
using kritva::core::ErrorCode;
using kritva::core::LifecycleState;

static void test_create_requires_a_node_id() {
    kritva::hardware::DeviceRegistry registry;
    SimulatedTransport link;
    KRITVA_CHECK(!EdgeHost::create(NodeId{}, registry, link.edge()).has_value());
    KRITVA_CHECK(EdgeHost::create(NodeId{1}, registry, link.edge()).has_value());
}

static void test_no_session_before_hello() {                           // ER-001
    EdgeRig rig;
    KRITVA_CHECK(!rig.edge->session().valid() && !rig.edge->sealed());
    rig.send(MessageType::DISCOVERY_REQUEST, {});
    rig.send(MessageType::READ_REQUEST, addr(kImu, kAccel));
    rig.send(MessageType::WRITE_REQUEST, must(encode(WriteRequestPayload{AddressPayload{kEdgeNode, kMotor, kCommand}, 0.1})));
    rig.send_raw(EdgeRig::frame(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{5})), 9, 4));
    KRITVA_CHECK(rig.poll().empty());                                     // silence: nothing to answer with
    KRITVA_CHECK(rig.stats().no_session == 4 && !rig.edge->sealed() && !rig.registry.closed());
    KRITVA_CHECK(rig.motor.model().velocity_rad_s == 0.0);
}

static void test_hello_accepted() {                                    // PR-001, ER-001
    EdgeRig rig;
    rig.send_raw(EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 1, 0, 50, 200})), 1, 0));
    const auto replies = rig.poll();
    KRITVA_CHECK(replies.size() == 1);
    const Reply& r = replies[0];
    KRITVA_CHECK(r.header.type == MessageType::HELLO_ACK && r.header.correlation_id == 1 && r.header.sequence == 1 && r.header.session_id == 1);
    const auto ack = decode<HelloAckPayload>(r.payload).value;
    KRITVA_CHECK(ack.status.ok() && ack.node_id == kEdgeNode && ack.major == 1 && ack.minor == 0 && ack.session_id == 1);
    KRITVA_CHECK(ack.heartbeat_period_ms == 50 && ack.heartbeat_timeout_ms == 200);       // the accepted timing is echoed
    KRITVA_CHECK(rig.edge->session().value() == 1 && rig.edge->timing().heartbeat_period_ms == 50 && rig.edge->timing().heartbeat_timeout_ms == 200);
    KRITVA_CHECK(rig.edge->sealed() && rig.registry.closed() && rig.imu.sealed() && rig.motor.sealed());   // sealed at the first HELLO
    KRITVA_CHECK(rig.imu.add_endpoint(std::make_unique<kritva::hardware::mock::MockAccelerationEndpoint>(kritva::hardware::EndpointId{9}, "late")).error().code == ErrorCode::INVALID_STATE);
    KRITVA_CHECK(rig.stats().hellos_accepted == 1 && rig.stats().actuators_stopped_on_new_session == 0);
}

static void test_hello_minor_negotiation() {
    EdgeRig rig;
    const auto ack = rig.hello(1, 0);
    KRITVA_CHECK(ack.status.ok() && ack.major == 1 && ack.minor == 0);     // the negotiated minor is the peer's
}

// Each rejected HELLO answers once, unsessioned (session 0), and leaves everything else alone.
struct Snapshot {
    std::uint64_t session;
    LifecycleState command, position, accel;
    double velocity;
    std::uint64_t ops_ok;
    std::uint64_t accepted;
    std::uint64_t last_valid;
    bool operator==(const Snapshot&) const = default;
};
static Snapshot snapshot(EdgeRig& rig) {
    return {rig.edge->session().value(), rig.motor.command().lifecycle_state(), rig.motor.position().lifecycle_state(),
            rig.imu.acceleration().lifecycle_state(), rig.motor.model().velocity_rad_s,
            rig.motor.command().statistics().sample_count.value(), rig.stats().hellos_accepted, rig.edge->last_valid_frame_ns()};
}

static void test_rejected_hello_has_no_effect_on_the_session() {       // architect ruling: a rejected HELLO changes nothing
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
    KRITVA_CHECK(rig.link.advance(1000).has_value());                      // so that a changed last-valid time would show
    const Snapshot before = snapshot(rig);
    const std::uint64_t applied_seq = rig.seq;

    struct Case { const char* what; Bytes bytes; ErrorCode code; bool protocol_error; };
    const Bytes ok_payload = must(encode(HelloPayload{kNexusNode, 1, 0, 100, 300}));
    const std::vector<Case> cases = {
        {"version: major 2", EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 2, 0, 100, 300})), 1, 0), ErrorCode::UNSUPPORTED, false},
        {"version: minor above ours", EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 1, 1, 100, 300})), 1, 0), ErrorCode::UNSUPPORTED, false},
        {"timing: period too small", EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 1, 0, 9, 300})), 1, 0), ErrorCode::INVALID_ARGUMENT, false},
        {"timing: timeout below 2 x period", EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 1, 0, 200, 399})), 1, 0), ErrorCode::INVALID_ARGUMENT, false},
        {"timing: period 60001", EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 1, 0, 60001, 600000})), 1, 0), ErrorCode::INVALID_ARGUMENT, false},
        {"timing: zero", EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 1, 0, 0, 0})), 1, 0), ErrorCode::INVALID_ARGUMENT, false},
        {"header: sequence 2", EdgeRig::frame(MessageType::HELLO, ok_payload, 2, 0), ErrorCode::INVALID_ARGUMENT, false},
        {"header: sequence 0", EdgeRig::frame(MessageType::HELLO, ok_payload, 0, 0), ErrorCode::INVALID_ARGUMENT, false},
        {"header: session id set", EdgeRig::frame(MessageType::HELLO, ok_payload, 1, 1), ErrorCode::INVALID_ARGUMENT, false},
        {"payload: truncated", EdgeRig::frame(MessageType::HELLO, Bytes(ok_payload.begin(), ok_payload.end() - 1), 1, 0), ErrorCode::INVALID_ARGUMENT, true},
        {"payload: trailing byte", EdgeRig::frame(MessageType::HELLO, [&] { Bytes b = ok_payload; b.push_back(0); return b; }(), 1, 0), ErrorCode::INVALID_ARGUMENT, true},
        {"payload: node id 0", EdgeRig::frame(MessageType::HELLO, [&] { Bytes b = ok_payload; for (int i = 0; i < 8; ++i) b[i] = 0; return b; }(), 1, 0), ErrorCode::INVALID_ARGUMENT, true},
    };
    std::uint64_t rejected = 0;
    for (const Case& c : cases) {
        rig.send_raw(c.bytes);
        const auto replies = rig.poll();
        KRITVA_CHECK(replies.size() == 1);                                 // exactly one answer, no amplification
        KRITVA_CHECK(replies[0].header.session_id == 0 && replies[0].header.correlation_id == decode_frame(c.bytes).header.sequence);   // unsessioned, correlated to the HELLO
        if (c.protocol_error) {
            KRITVA_CHECK(replies[0].header.type == MessageType::PROTOCOL_ERROR);
            KRITVA_CHECK(decode_protocol_error(replies[0].payload).value.status.code == c.code);
        } else {
            KRITVA_CHECK(replies[0].header.type == MessageType::HELLO_ACK);
            const auto ack = decode<HelloAckPayload>(replies[0].payload).value;
            KRITVA_CHECK(!ack.status.ok() && ack.status.code == c.code && ack.session_id == 0);
        }
        KRITVA_CHECK(snapshot(rig) == before);                             // not the session, the actuator, the ledger or the clock reading
        ++rejected;
    }
    KRITVA_CHECK(rig.stats().hellos_rejected == rejected && rig.stats().actuators_stopped_on_new_session == 0);

    // The session is fully alive: the ledger still resends the last applied write, a stale frame is still stale, and new work is admitted.
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, must(encode(WriteRequestPayload{AddressPayload{kEdgeNode, kMotor, kCommand}, 0.5})), applied_seq, rig.session));
    KRITVA_CHECK(rig.poll().size() == 1 && rig.stats().writes_resent == 1);
    KRITVA_CHECK(rig.write(kMotor, kCommand, -0.25).status.ok() && rig.motor.model().velocity_rad_s == -0.25);
}

static void test_hello_replaces_the_session_as_a_safety_action() {      // protocol section 9
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    const std::uint64_t first = rig.session;
    rig.bring_up_all();
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
    const std::uint64_t old_seq = rig.seq;
    KRITVA_CHECK(rig.motor.command().lifecycle_state() == LifecycleState::RUNNING && rig.motor.command().effective_velocity() == 0.5);

    const auto ack = rig.hello();                                          // a new session
    KRITVA_CHECK(ack.status.ok() && ack.session_id == first + 1 && rig.edge->session().value() == first + 1);
    // (1)+(2): the actuator was stopped through Endpoint::stop(); sensors keep running.
    KRITVA_CHECK(rig.motor.command().lifecycle_state() == LifecycleState::STOPPED && rig.motor.command().effective_velocity() == 0.0);
    KRITVA_CHECK(rig.imu.acceleration().lifecycle_state() == LifecycleState::RUNNING && rig.motor.position().lifecycle_state() == LifecycleState::RUNNING);
    KRITVA_CHECK(rig.stats().actuators_stopped_on_new_session == 1 && rig.stats().actuator_stop_failures == 0);
    // (3): no ledger or watermark is inherited.
    const Bytes old_write = EdgeRig::frame(MessageType::WRITE_REQUEST, must(encode(WriteRequestPayload{AddressPayload{kEdgeNode, kMotor, kCommand}, 0.5})), old_seq, first);
    rig.send_raw(old_write);                                               // a retransmission from the old session
    KRITVA_CHECK(rig.poll().empty() && rig.stats().wrong_session == 1);    // dropped: it can never be applied
    // The new session's own sequence 2 is admitted although the old session had reached a higher number.
    KRITVA_CHECK(rig.lifecycle(MessageType::START_REQUEST, kMotor, kCommand).status.code == ErrorCode::INVALID_STATE);   // answered (STOPPED needs initialize)
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kMotor, kCommand).status.ok());
    KRITVA_CHECK(rig.lifecycle(MessageType::START_REQUEST, kMotor, kCommand).status.ok());
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok() && rig.stats().writes_applied == 2);   // the same value applies again: a fresh ledger
}

static void test_first_hello_stops_nothing() {
    EdgeRig rig;
    // An actuator that the integrator started locally before any session is not "of the previous session".
    kritva::core::Configuration cfg;
    KRITVA_CHECK(rig.motor.command().configure(cfg).has_value() && rig.motor.command().initialize().has_value() && rig.motor.command().start().has_value());
    KRITVA_CHECK(rig.hello().status.ok());
    KRITVA_CHECK(rig.motor.command().lifecycle_state() == LifecycleState::RUNNING && rig.stats().actuators_stopped_on_new_session == 0);
}

static void test_duplicated_hello_replaces_the_session() {              // Protocol 1.0: HELLO is not idempotent
    EdgeRig rig;
    const Bytes hello = EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 1, 0, 100, 300})), 1, 0);
    rig.send_raw(hello);
    auto replies = rig.poll();
    KRITVA_CHECK(replies.size() == 1 && replies[0].header.session_id == 1);
    const std::uint64_t first_session = 1;
    rig.send_raw(hello);                                                   // the same HELLO bytes again (for example a link duplicate)
    replies = rig.poll();
    KRITVA_CHECK(replies.size() == 1 && replies[0].header.session_id == 2);
    KRITVA_CHECK(rig.edge->session().value() == 2 && rig.stats().hellos_accepted == 2);
    // The Nexus still holds session 1: everything it sends is wrong-session, and nothing is applied.
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 2, first_session));
    KRITVA_CHECK(rig.poll().empty() && rig.stats().wrong_session == 1);
}

static void test_wrong_session_and_stale_are_dropped_silently() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 2, rig.session + 1));   // another session
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 2, 0));                 // no session id
    KRITVA_CHECK(rig.poll().empty() && rig.stats().wrong_session == 2);
    KRITVA_CHECK(rig.discover().status.ok());                                               // sequence 2 still unused: nothing above raised the watermark
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 2, rig.session));       // a repeat of sequence 2
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 1, rig.session));       // older
    KRITVA_CHECK(rig.poll().empty() && rig.stats().stale_frames == 2);
}

static void test_the_hello_sequence_is_not_replayable_as_a_request() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 1, rig.session));       // sequence 1 belongs to the HELLO
    KRITVA_CHECK(rig.poll().empty() && rig.stats().stale_frames == 1);
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 2, rig.session));
    KRITVA_CHECK(rig.poll().size() == 1);
}

static void test_replacement_stops_only_running_actuators() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kMotor, kCommand).status.ok());   // READY, not RUNNING: it drives nothing
    KRITVA_CHECK(rig.hello().status.ok());
    KRITVA_CHECK(rig.motor.command().lifecycle_state() == LifecycleState::READY);
    KRITVA_CHECK(rig.stats().actuators_stopped_on_new_session == 0 && rig.stats().actuator_stop_failures == 0);
    // An actuator that was never initialized is left alone too, and nothing is counted as a failed stop.
    KRITVA_CHECK(rig.motor.position().lifecycle_state() == LifecycleState::UNKNOWN);
}

static void test_a_failing_stop_is_counted_and_does_not_block_the_others() {
    using namespace kritva::hardware;
    EdgeRig rig(false);
    Device bad(DeviceInfo::create(DeviceId{3}, "bad").value());
    auto owned = std::make_unique<kritva::hardware::test::TestActuator>(1, "act");
    kritva::hardware::test::TestActuator* act = owned.get();
    KRITVA_CHECK(bad.add_endpoint(std::move(owned)).has_value());
    KRITVA_CHECK(rig.registry.register_device(bad).has_value() && rig.registry.register_device(rig.motor).has_value());   // the failing one first
    KRITVA_CHECK(rig.hello().status.ok());
    KRITVA_CHECK(act->configure(kritva::core::Configuration{}).has_value() && act->initialize().has_value() && act->start().has_value());
    act->fail_at = kritva::hardware::test::FailAt::STOP;
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kMotor, kCommand).status.ok() && rig.lifecycle(MessageType::START_REQUEST, kMotor, kCommand).status.ok());
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
    KRITVA_CHECK(rig.hello().status.ok());
    KRITVA_CHECK(act->lifecycle_state() == LifecycleState::FAULT);                          // I3: a failing stop is FAULT
    KRITVA_CHECK(rig.motor.command().lifecycle_state() == LifecycleState::STOPPED && rig.motor.command().effective_velocity() == 0.0);   // the others were still stopped
    KRITVA_CHECK(rig.stats().actuator_stop_failures == 1 && rig.stats().actuators_stopped_on_new_session == 1);
}

static void test_sequence_watermark_allows_gaps_and_is_per_session() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 10, rig.session));       // a gap is fine
    KRITVA_CHECK(rig.poll().size() == 1);
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 9, rig.session));        // reordered: stale
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 11, rig.session));
    KRITVA_CHECK(rig.poll().size() == 1 && rig.stats().stale_frames == 1);
    KRITVA_CHECK(rig.hello().status.ok());                                                    // new session: the watermark restarts
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 2, rig.session));
    KRITVA_CHECK(rig.poll().size() == 1);
}

static void test_a_response_like_frame_does_not_touch_the_watermark() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    const Bytes err = must(encode_protocol_error(StatusPayload{{ErrorCode::INVALID_ARGUMENT, "x"}}));
    rig.send_raw(EdgeRig::frame(MessageType::PROTOCOL_ERROR, err, 50, rig.session, 7));      // correlation != 0: response-like, the Edge asked nothing
    KRITVA_CHECK(rig.poll().empty() && rig.stats().unexpected_frames == 1);
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 2, rig.session));        // sequence 2 is still admitted
    KRITVA_CHECK(rig.poll().size() == 1);
    rig.send_raw(EdgeRig::frame(MessageType::PROTOCOL_ERROR, err, 3, rig.session, 0));       // correlation 0: a notice, admitted, nothing answered
    KRITVA_CHECK(rig.poll().empty() && rig.stats().stale_frames == 0);
    rig.send_raw(EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 3, rig.session));        // the notice used sequence 3
    KRITVA_CHECK(rig.poll().empty() && rig.stats().stale_frames == 1);
}

static void test_header_violations_are_dropped_silently() {              // FR-003, decision 9
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    const Bytes good = EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, 2, rig.session);
    std::vector<Bytes> bad;
    { Bytes b = good; b[0] ^= 0xFF; bad.push_back(b); }                                       // magic
    { Bytes b = good; b[4] = 2; bad.push_back(b); }                                           // major version
    { Bytes b = good; b[10] = 1; bad.push_back(b); }                                          // flags
    { Bytes b = good; b[12] = 45; bad.push_back(b); }                                         // header length
    { Bytes b = good; b[14] = 1; bad.push_back(b); }                                          // reserved
    { Bytes b = good; b.push_back(0); bad.push_back(b); }                                     // length mismatch
    { Bytes b = good; b.pop_back(); bad.push_back(b); }                                       // truncated
    { Bytes b = good; b[8] = 0xEE; b[9] = 0x7F; bad.push_back(b); }                           // unknown type
    bad.push_back(Bytes(10, 0xAA));                                                           // garbage
    bad.push_back(EdgeRig::frame(MessageType::READ_RESPONSE, must(encode(StatusPayload{})), 3, rig.session, 2));   // a response type sent to the Edge
    bad.push_back(EdgeRig::frame(MessageType::FAULT_EVENT, must(encode(FaultEventPayload{AddressPayload{kEdgeNode, 1, 1}, LifecycleState::FAULT, "x"})), 4, rig.session));   // Edge-to-Nexus only
    for (const Bytes& b : bad) {
        rig.link.nexus().send(b.empty() ? Bytes{0} : b);
        KRITVA_CHECK(rig.poll().empty());                                                     // no response at all
    }
    KRITVA_CHECK(rig.stats().frame_errors == bad.size());
    KRITVA_CHECK(rig.discover().status.ok());                                                 // nothing above raised the watermark: sequence 2 works
}

static void test_a_malformed_request_gets_one_protocol_error_and_changes_nothing() {   // decision 9
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    const std::uint64_t seq = rig.seq + 1;
    const Bytes good_write = must(encode(WriteRequestPayload{AddressPayload{kEdgeNode, kMotor, kCommand}, 0.5}));
    std::vector<std::pair<MessageType, Bytes>> bad = {
        {MessageType::DISCOVERY_REQUEST, Bytes{1}},                                           // must be empty
        {MessageType::READ_REQUEST, Bytes(5, 0)},
        {MessageType::OBSERVE_REQUEST, addr(kImu, kAccel)},                                   // placeholder replaced below
        {MessageType::INITIALIZE_REQUEST, Bytes(24, 0)},                                      // zero ids
        {MessageType::START_REQUEST, [&] { Bytes b = addr(kMotor, kCommand); b.push_back(0); return b; }()},   // trailing byte
        {MessageType::CONFIGURE_REQUEST, Bytes(3, 1)},
        {MessageType::WRITE_REQUEST, Bytes(good_write.begin(), good_write.end() - 1)},        // truncated
    };
    bad[2].second = Bytes(10, 0);
    { Bytes nan = good_write; for (int i = 0; i < 8; ++i) nan[24 + i] = 0xFF; bad.push_back({MessageType::WRITE_REQUEST, nan}); }   // a NaN velocity
    for (const auto& [type, payload] : bad) {
        const std::uint64_t s = rig.send(type, payload);
        const auto replies = rig.poll();
        KRITVA_CHECK(replies.size() == 1 && replies[0].header.type == MessageType::PROTOCOL_ERROR && replies[0].header.correlation_id == s);
        KRITVA_CHECK(decode_protocol_error(replies[0].payload).value.status.code == ErrorCode::INVALID_ARGUMENT);
        --rig.seq;                                                                           // a malformed frame did not raise the watermark: reuse the number
    }
    KRITVA_CHECK(rig.stats().protocol_errors_sent == bad.size());
    KRITVA_CHECK(rig.seq + 1 == seq && rig.motor.model().velocity_rad_s == 0.0 && rig.stats().writes_applied == 0);
    KRITVA_CHECK(rig.discover().status.ok());                                                 // the sequence is still free for a good frame
}

static void test_heartbeats() {                                          // PR-002
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    KRITVA_CHECK(rig.edge->last_valid_frame_ns() == 0);
    KRITVA_CHECK(rig.link.advance(4000).has_value());
    rig.send(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{4000})));
    KRITVA_CHECK(rig.poll().empty() && rig.stats().heartbeats == 1 && rig.edge->last_valid_frame_ns() == 4000);   // recorded, never answered
    KRITVA_CHECK(rig.link.advance(1000).has_value());
    rig.send_raw(EdgeRig::frame(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{5000})), rig.seq, rig.session));   // stale: does not count as liveness
    KRITVA_CHECK(rig.poll().empty() && rig.edge->last_valid_frame_ns() == 4000 && rig.stats().stale_frames == 1);
    rig.send_raw(EdgeRig::frame(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{5000})), rig.seq + 1, rig.session + 1));   // another session
    KRITVA_CHECK(rig.poll().empty() && rig.edge->last_valid_frame_ns() == 4000);
    rig.send_raw(EdgeRig::frame(MessageType::HEARTBEAT, Bytes(3, 0), rig.seq + 1, rig.session));   // malformed: dropped, no PROTOCOL_ERROR for a notice
    KRITVA_CHECK(rig.poll().empty() && rig.edge->last_valid_frame_ns() == 4000);
    // Any valid in-session frame is proof of liveness.
    rig.send(MessageType::DISCOVERY_REQUEST, {});
    KRITVA_CHECK(rig.poll().size() == 1 && rig.edge->last_valid_frame_ns() == 5000);
}

static void test_discovery_snapshot() {                                  // ER-002
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    const auto d = rig.discover();
    KRITVA_CHECK(d.status.ok() && d.devices.size() == 2);
    KRITVA_CHECK(d.devices[0].id == kImu && d.devices[0].name == "imu" && d.devices[0].endpoints.size() == 2);
    KRITVA_CHECK(d.devices[1].id == kMotor && d.devices[1].name == "motor" && d.devices[1].endpoints.size() == 2);
    const auto& a = d.devices[0].endpoints[0];
    KRITVA_CHECK(a.id == kAccel && a.name == "acceleration" && a.direction == WireDirection::SENSOR && a.capabilities.size() == 1 &&
                 a.capabilities[0].id == 0x1001 && a.capabilities[0].name == "acceleration");
    KRITVA_CHECK(d.devices[0].endpoints[1].capabilities[0].id == 0x1002);
    const auto& c = d.devices[1].endpoints[0];
    KRITVA_CHECK(c.id == kCommand && c.direction == WireDirection::ACTUATOR && c.capabilities[0].id == 0x2001);
    KRITVA_CHECK(d.devices[1].endpoints[1].capabilities[0].id == 0x1003 && d.devices[1].endpoints[1].direction == WireDirection::SENSOR);
    // Explicit re-discovery returns the same complete snapshot, in any lifecycle state.
    rig.bring_up_all();
    const auto again = rig.discover();
    KRITVA_CHECK(again.status.ok() && again.devices.size() == 2 && encode(again).value() == encode(d).value());
}

static void test_discovery_of_an_empty_registry() {
    EdgeRig rig(false);
    KRITVA_CHECK(rig.hello().status.ok());
    const auto d = rig.discover();
    KRITVA_CHECK(d.status.ok() && d.devices.empty());
}

static void test_discovery_refuses_an_unsupported_topology() {           // spec section 13: anything else is UNSUPPORTED
    using namespace kritva::hardware;
    // A typed-looking capability on an object that is not the typed endpoint, a sensor of an unknown kind, an endpoint with two capabilities.
    struct Odd final : Device {
        explicit Odd(DeviceInfo i) : Device(std::move(i)) {}
    };
    EdgeRig rig(false);
    Odd odd(DeviceInfo::create(DeviceId{5}, "odd").value());
    KRITVA_CHECK(odd.add_endpoint(std::make_unique<kritva::hardware::test::TestSensor>(0x1001, "fake")).has_value());   // 0x1001, but not an AccelerationEndpoint
    KRITVA_CHECK(rig.registry.register_device(rig.imu).has_value() && rig.registry.register_device(odd).has_value());
    KRITVA_CHECK(rig.hello().status.ok());
    const auto d = rig.discover();
    KRITVA_CHECK(!d.status.ok() && d.status.code == ErrorCode::UNSUPPORTED && d.devices.empty());   // serves no topology at all
    // The refusal does not break the session.
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kImu, kAccel).status.ok());
}

int main() {
    test_create_requires_a_node_id();
    test_no_session_before_hello();
    test_hello_accepted();
    test_hello_minor_negotiation();
    test_rejected_hello_has_no_effect_on_the_session();
    test_hello_replaces_the_session_as_a_safety_action();
    test_first_hello_stops_nothing();
    test_duplicated_hello_replaces_the_session();
    test_wrong_session_and_stale_are_dropped_silently();
    test_the_hello_sequence_is_not_replayable_as_a_request();
    test_replacement_stops_only_running_actuators();
    test_a_failing_stop_is_counted_and_does_not_block_the_others();
    test_sequence_watermark_allows_gaps_and_is_per_session();
    test_a_response_like_frame_does_not_touch_the_watermark();
    test_header_violations_are_dropped_silently();
    test_a_malformed_request_gets_one_protocol_error_and_changes_nothing();
    test_heartbeats();
    test_discovery_snapshot();
    test_discovery_of_an_empty_registry();
    test_discovery_refuses_an_unsupported_topology();
    std::printf("edge_session_test: PASS\n");
    return 0;
}
