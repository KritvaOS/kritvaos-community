//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : edge_supervision_test.cpp
// Description : Unit tests of the Edge supervision (protocol section 11): heartbeat transmission, the heartbeat timeout,
//               which frames count as liveness, the actuator stop, DEGRADED, and FAULT_EVENT.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: SR-001; SR-002; SR-003; FRL-001; FRL-003 (HUMAN SAFETY REVIEW OPEN)
// API         : EDGE-SUPERVISION-UT
//
// Author      : KritvaOS
// Created     : 06-10-2026
//==============================================================================

#include <functional>

#include "../support/edge_rig.hpp"
#include "../support/test_endpoints.hpp"

using namespace kritva::hardware::remote;
using namespace kritva::hardware::remote::test;
using kritva::core::ErrorCode;
using kritva::core::LifecycleState;

static constexpr std::uint64_t kMs = 1'000'000;

static void advance(EdgeRig& rig, std::uint64_t ns) { KRITVA_CHECK(rig.link.advance(ns).has_value()); }

static std::vector<Reply> of_type(const std::vector<Reply>& all, MessageType t) {
    std::vector<Reply> out;
    for (const Reply& r : all) if (r.header.type == t) out.push_back(r);
    return out;
}

static Bytes write_payload(double v) {
    return must(encode(WriteRequestPayload{AddressPayload{kEdgeNode, kMotor, kCommand}, v}));
}

static bool motor_running(EdgeRig& rig) {
    return rig.motor.command().lifecycle_state() == LifecycleState::RUNNING;
}

// ---- heartbeat transmission ----------------------------------------------------------------------------------------

static void test_the_edge_sends_one_heartbeat_per_period() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());                                          // t = 0, the HELLO_ACK was sequence 1
    advance(rig, 100 * kMs - 1);
    KRITVA_CHECK(rig.poll().empty());                                               // one nanosecond before the period: nothing
    advance(rig, 1);
    auto r = rig.poll();
    KRITVA_CHECK(r.size() == 1 && r[0].header.type == MessageType::HEARTBEAT && r[0].header.session_id == rig.session && r[0].header.correlation_id == 0);
    KRITVA_CHECK(r[0].header.sequence == 2 && decode<HeartbeatPayload>(r[0].payload).value.sender_time_ns == 100 * kMs);
    KRITVA_CHECK(rig.poll().empty() && rig.stats().heartbeats_sent == 1);           // not again at the same time
    // With a live Nexus (its own heartbeats), one per period for as long as it lasts.
    for (int i = 0; i < 5; ++i) {
        advance(rig, 100 * kMs);
        rig.send(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{static_cast<std::uint64_t>(i)})));
        r = rig.poll();
        KRITVA_CHECK(r.size() == 1 && r[0].header.type == MessageType::HEARTBEAT && r[0].header.sequence == 3 + static_cast<std::uint64_t>(i));
    }
    KRITVA_CHECK(rig.stats().heartbeats_sent == 6 && rig.stats().sessions_timed_out == 0);
}

static void test_a_jump_in_time_is_not_a_heartbeat_burst() {                       // approved constraint E3
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    advance(rig, 250 * kMs);                                                        // two and a half periods in one drive
    KRITVA_CHECK(rig.poll().size() == 1 && rig.stats().heartbeats_sent == 1);
    KRITVA_CHECK(rig.poll().empty() && rig.stats().heartbeats_sent == 1);
}

static void test_a_heartbeat_never_acts() {                                        // approved constraint E4
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
    const auto stats = rig.motor.command().statistics().sample_count.value();
    advance(rig, 150 * kMs);
    KRITVA_CHECK(of_type(rig.poll(), MessageType::HEARTBEAT).size() == 1);
    KRITVA_CHECK(motor_running(rig) && rig.motor.model().velocity_rad_s == 0.5 && rig.motor.command().statistics().sample_count.value() == stats);
}

// ---- the heartbeat timeout -----------------------------------------------------------------------------------------

static void test_the_timeout_is_exactly_at_the_bound_and_stops_only_actuators() {   // SR-003, approved constraint E1
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());                       // the last liveness is at t = 0
    advance(rig, 300 * kMs - 1);
    rig.poll();
    KRITVA_CHECK(rig.edge->session().valid() && motor_running(rig) && rig.motor.model().velocity_rad_s == 0.5 && rig.stats().sessions_timed_out == 0);
    KRITVA_CHECK(rig.edge->link_state() == SessionState::DEGRADED);                  // 2 periods without liveness: observation only
    advance(rig, 1);                                                                 // exactly the timeout
    rig.poll();
    KRITVA_CHECK(!rig.edge->session().valid() && rig.edge->link_state() == SessionState::DISCONNECTED);
    KRITVA_CHECK(rig.motor.command().lifecycle_state() == LifecycleState::STOPPED && rig.motor.command().effective_velocity() == 0.0);
    KRITVA_CHECK(rig.stats().sessions_timed_out == 1 && rig.stats().actuators_stopped_on_timeout == 1 && rig.stats().actuator_stop_failures == 0);
    KRITVA_CHECK(rig.stats().actuators_stopped_on_new_session == 0 && rig.edge->timing().heartbeat_period_ms == 0 && rig.edge->timing().heartbeat_timeout_ms == 0);
    // Sensors keep their state; no Edge endpoint is faulted by the loss of the link.
    KRITVA_CHECK(rig.imu.acceleration().lifecycle_state() == LifecycleState::RUNNING && rig.imu.angular_velocity().lifecycle_state() == LifecycleState::RUNNING &&
                 rig.motor.position().lifecycle_state() == LifecycleState::RUNNING);
}

static void test_degraded_is_observation_only_and_a_frame_ends_it() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
    KRITVA_CHECK(rig.edge->link_state() == SessionState::CONNECTED);
    advance(rig, 199 * kMs);
    rig.poll();
    KRITVA_CHECK(rig.edge->link_state() == SessionState::CONNECTED);
    advance(rig, 1 * kMs);
    rig.poll();
    KRITVA_CHECK(rig.edge->link_state() == SessionState::DEGRADED && motor_running(rig) && rig.motor.command().effective_velocity() == 0.5);   // never acts
    rig.send(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1})));
    rig.poll();
    KRITVA_CHECK(rig.edge->link_state() == SessionState::CONNECTED);                 // a valid frame ends DEGRADED
    advance(rig, 299 * kMs);
    rig.poll();
    KRITVA_CHECK(rig.edge->session().valid() && motor_running(rig));                  // the timeout counts from the refreshing frame
}

static void test_supervision_runs_only_when_driven() {                             // approved constraint E2
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
    advance(rig, 10'000 * kMs);                                                      // ten virtual seconds, nobody drives the Edge
    KRITVA_CHECK(rig.edge->session().valid() && motor_running(rig) && rig.stats().sessions_timed_out == 0 && rig.stats().heartbeats_sent == 0);
    KRITVA_CHECK(rig.edge->poll() == 0);
    KRITVA_CHECK(!rig.edge->session().valid() && !motor_running(rig) && rig.stats().sessions_timed_out == 1);
}

static void test_the_old_session_is_gone_for_good() {                              // approved constraints E5, E6
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    const std::uint64_t old_session = rig.session;
    rig.bring_up_all();
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
    const std::uint64_t last_write = rig.seq;
    advance(rig, 300 * kMs);
    rig.poll();
    KRITVA_CHECK(!rig.edge->session().valid());
    // A heartbeat of the old session, even right now, revives nothing; neither does a replay of its last write.
    rig.send_raw(EdgeRig::frame(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1})), rig.seq + 1, old_session));
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.5), last_write, old_session));
    KRITVA_CHECK(rig.poll().empty() && !rig.edge->session().valid() && rig.stats().no_session == 2 && rig.stats().writes_resent == 0);
    // Only a new HELLO starts a session, with a new id, a new ledger and a new watermark; the old id never becomes valid.
    KRITVA_CHECK(rig.hello().status.ok() && rig.session != old_session && rig.edge->session().value() == old_session + 1);
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.5), last_write + 5, old_session));
    KRITVA_CHECK(rig.poll().empty() && rig.stats().wrong_session == 1);
    KRITVA_CHECK(!motor_running(rig));                                               // nothing started the actuator again
}

static void test_a_new_hello_just_before_the_timeout_takes_the_replacement_path() {   // approved constraint E6
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
    advance(rig, 299 * kMs);
    KRITVA_CHECK(rig.hello().status.ok());                                           // the old session is replaced, not timed out
    KRITVA_CHECK(rig.stats().sessions_timed_out == 0 && rig.stats().actuators_stopped_on_new_session == 1 && !motor_running(rig));
    advance(rig, 299 * kMs);
    rig.poll();
    KRITVA_CHECK(rig.edge->session().valid() && rig.stats().sessions_timed_out == 0);   // the new session's clock started with its HELLO
    advance(rig, 1 * kMs);
    rig.poll();
    KRITVA_CHECK(!rig.edge->session().valid() && rig.stats().sessions_timed_out == 1);
}

static void test_timeout_stops_every_actuator_and_counts_a_failed_stop() {
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
    advance(rig, 300 * kMs);
    rig.poll();
    KRITVA_CHECK(act->lifecycle_state() == LifecycleState::FAULT);                  // the I3 rule for a failing stop
    KRITVA_CHECK(rig.motor.command().lifecycle_state() == LifecycleState::STOPPED && rig.motor.command().effective_velocity() == 0.0);   // the others are still stopped
    KRITVA_CHECK(rig.stats().actuator_stop_failures == 1 && rig.stats().actuators_stopped_on_timeout == 1);
    KRITVA_CHECK(rig.stats().fault_events_sent == 0 && rig.stats().fault_events_dropped == 1);   // no session is left to tell
}

// ---- which frames count as liveness ----------------------------------------------------------------------------------

struct LivenessCase {
    const char* what;
    bool refreshes;
    std::function<void(EdgeRig&, std::uint64_t last_write)> action;
};

// The frame arrives at t = 200 ms, the last liveness being at t = 0. If it counts, the session lives until 500 ms and is
// alive at 400 ms; if not, it ends at 300 ms and is gone by 400 ms.
static void test_which_frames_refresh_liveness() {                                  // approved table E11
    const Bytes err = must(encode_protocol_error(StatusPayload{{ErrorCode::INVALID_ARGUMENT, "x"}}));
    const std::vector<LivenessCase> cases = {
        {"a new admitted request", true, [](EdgeRig& r, std::uint64_t) { r.send(MessageType::DISCOVERY_REQUEST, {}); }},
        {"an admitted heartbeat", true, [](EdgeRig& r, std::uint64_t) { r.send(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1}))); }},
        {"a new notice (PROTOCOL_ERROR with correlation 0)", true, [&](EdgeRig& r, std::uint64_t) { r.send(MessageType::PROTOCOL_ERROR, err); }},
        {"a valid request the endpoint refuses (limits)", true, [](EdgeRig& r, std::uint64_t) { r.send(MessageType::WRITE_REQUEST, write_payload(5.0)); }},
        {"a valid request for an unknown address", true, [](EdgeRig& r, std::uint64_t) { r.send(MessageType::READ_REQUEST, addr(99, 99)); }},
        {"a duplicate WRITE answered from the ledger", false, [](EdgeRig& r, std::uint64_t w) {
            r.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.5), w, r.session)); }},
        {"a stale WRITE", false, [](EdgeRig& r, std::uint64_t) { r.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.7), 1, r.session)); }},
        {"a stale heartbeat", false, [](EdgeRig& r, std::uint64_t) {
            r.send_raw(EdgeRig::frame(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1})), 1, r.session)); }},
        {"a request with a malformed payload", false, [](EdgeRig& r, std::uint64_t) { r.send(MessageType::READ_REQUEST, Bytes(3, 0)); }},
        {"a frame with a bad header", false, [](EdgeRig& r, std::uint64_t) {
            Bytes b = EdgeRig::frame(MessageType::DISCOVERY_REQUEST, {}, r.seq + 1, r.session); b[0] ^= 0xFF; r.send_raw(b); }},
        {"a frame of another session", false, [](EdgeRig& r, std::uint64_t) {
            r.send_raw(EdgeRig::frame(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1})), r.seq + 1, r.session + 1)); }},
        {"a frame that cannot reach an Edge (a response type)", false, [](EdgeRig& r, std::uint64_t) {
            r.send_raw(EdgeRig::frame(MessageType::READ_RESPONSE, must(encode(StatusPayload{})), r.seq + 1, r.session, 5)); }},
        {"a PROTOCOL_ERROR with a correlation id (a response the Edge never asked for)", false, [&](EdgeRig& r, std::uint64_t) {
            r.send_raw(EdgeRig::frame(MessageType::PROTOCOL_ERROR, err, r.seq + 1, r.session, 7)); }},
        {"a rejected HELLO (unsupported version)", false, [](EdgeRig& r, std::uint64_t) {
            r.send_raw(EdgeRig::frame(MessageType::HELLO, must(encode(HelloPayload{kNexusNode, 2, 0, 100, 300})), 1, 0)); }},
    };
    for (const LivenessCase& c : cases) {
        EdgeRig rig;
        KRITVA_CHECK(rig.hello().status.ok());
        rig.bring_up_all();
        KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
        const std::uint64_t last_write = rig.seq;
        KRITVA_CHECK(rig.edge->last_valid_frame_ns() == 0);
        advance(rig, 200 * kMs);
        c.action(rig, last_write);
        rig.poll();
        const std::uint64_t expected = c.refreshes ? 200 * kMs : 0;
        if (rig.edge->last_valid_frame_ns() != expected) {
            std::fprintf(stderr, "liveness: %s: last valid %llu, expected %llu\n", c.what,
                         static_cast<unsigned long long>(rig.edge->last_valid_frame_ns()), static_cast<unsigned long long>(expected));
            std::exit(1);
        }
        advance(rig, 200 * kMs);
        rig.poll();
        if (rig.edge->session().valid() != c.refreshes || motor_running(rig) != c.refreshes) {
            std::fprintf(stderr, "liveness: %s: session valid %d, expected %d\n", c.what, rig.edge->session().valid(), c.refreshes);
            std::exit(1);
        }
    }
}

// The fail-safe regression: replayed application traffic must never keep an actuator running.
static void test_replayed_writes_cannot_keep_an_actuator_alive() {                 // SR-001/SR-002, the I4-006 safety invariant
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());                      // applied once, sequence N
    const std::uint64_t n = rig.seq;
    const std::uint64_t applied = rig.motor.command().statistics().sample_count.value();
    // The Nexus is gone. A stuck retransmitter keeps sending the same frame, every 50 ms, for a second.
    for (int i = 0; i < 20; ++i) {
        advance(rig, 50 * kMs);
        rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.5), n, rig.session));
        rig.edge->poll();
        rig.drain();
    }
    KRITVA_CHECK(rig.stats().writes_resent > 0);                                     // the duplicates really arrived and were answered
    KRITVA_CHECK(!rig.edge->session().valid() && rig.motor.command().lifecycle_state() == LifecycleState::STOPPED);
    KRITVA_CHECK(rig.motor.command().effective_velocity() == 0.0 && rig.stats().sessions_timed_out == 1);
    KRITVA_CHECK(rig.stats().writes_applied == 1 && rig.motor.command().statistics().sample_count.value() == applied);   // applied exactly once
    // The same holds for a stuck stale heartbeat.
    EdgeRig rig2;
    KRITVA_CHECK(rig2.hello().status.ok());
    rig2.bring_up_all();
    rig2.send(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1})));
    rig2.poll();
    const std::uint64_t h = rig2.seq;
    KRITVA_CHECK(rig2.write(kMotor, kCommand, 0.5).status.ok());
    for (int i = 0; i < 20; ++i) {
        advance(rig2, 50 * kMs);
        rig2.send_raw(EdgeRig::frame(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1})), h, rig2.session));
        rig2.edge->poll();
        rig2.drain();
    }
    KRITVA_CHECK(!rig2.edge->session().valid() && !motor_running(rig2));
}

// ---- FAULT_EVENT --------------------------------------------------------------------------------------------------------

static void test_a_fault_outside_a_request_is_told_at_the_next_poll() {            // FRL-001
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    KRITVA_CHECK(rig.imu.acceleration().inject_fault("hardware fault").has_value());
    KRITVA_CHECK(rig.drain().empty());                                               // nothing is sent from inside the fault itself
    auto r = of_type(rig.poll(), MessageType::FAULT_EVENT);
    KRITVA_CHECK(r.size() == 1 && r[0].header.session_id == rig.session && r[0].header.correlation_id == 0);
    const auto e = decode<FaultEventPayload>(r[0].payload).value;
    KRITVA_CHECK(e.address.node_id == kEdgeNode && e.address.device_id == kImu && e.address.endpoint_id == kAccel &&
                 e.state == LifecycleState::FAULT && e.reason == "hardware fault");
    // A fault transition is told once: FAULT to FAULT is not a transition.
    KRITVA_CHECK(rig.imu.acceleration().inject_fault("again").has_value() && of_type(rig.poll(), MessageType::FAULT_EVENT).empty());
    KRITVA_CHECK(rig.stats().fault_events_sent == 1);
    // After the way back, a new fault is a new transition and a new event.
    KRITVA_CHECK(rig.lifecycle(MessageType::SHUTDOWN_REQUEST, kImu, kAccel).status.ok());
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kImu, kAccel).status.ok() && rig.lifecycle(MessageType::START_REQUEST, kImu, kAccel).status.ok());
    KRITVA_CHECK(rig.imu.acceleration().inject_fault("second").has_value());
    r = of_type(rig.poll(), MessageType::FAULT_EVENT);
    KRITVA_CHECK(r.size() == 1 && decode<FaultEventPayload>(r[0].payload).value.reason == "second" && rig.stats().fault_events_sent == 2);
}

static void test_a_fault_caused_by_a_request_is_told_after_its_response() {        // approved constraint C
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    const Reply c = rig.call(MessageType::CONFIGURE_REQUEST, must(encode(ConfigureRequestPayload{AddressPayload{kEdgeNode, kMotor, kCommand}, {{"fault_after_writes", std::int64_t{1}}}})));
    KRITVA_CHECK(decode<LifecycleResponsePayload>(c.payload).value.status.ok());
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kMotor, kCommand).status.ok() && rig.lifecycle(MessageType::START_REQUEST, kMotor, kCommand).status.ok());
    rig.send(MessageType::WRITE_REQUEST, write_payload(0.5));
    const auto all = rig.poll();
    KRITVA_CHECK(all.size() == 2 && all[0].header.type == MessageType::WRITE_RESPONSE && all[1].header.type == MessageType::FAULT_EVENT);   // the answer first
    KRITVA_CHECK(all[1].header.sequence > all[0].header.sequence);
    KRITVA_CHECK(decode<StatusPayload>(all[0].payload).value.status.ok());           // the write itself succeeded; the fault came after it
}

static void test_a_failing_lifecycle_request_is_answered_then_the_fault_is_told() {
    using namespace kritva::hardware;
    EdgeRig rig(false);
    Device dev(DeviceInfo::create(DeviceId{3}, "dev").value());
    auto owned = std::make_unique<kritva::hardware::test::TestActuator>(1, "act");
    kritva::hardware::test::TestActuator* act = owned.get();
    KRITVA_CHECK(dev.add_endpoint(std::move(owned)).has_value() && rig.registry.register_device(dev).has_value());
    KRITVA_CHECK(rig.hello().status.ok());
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, 3, 1).status.ok());
    act->fail_at = kritva::hardware::test::FailAt::START;
    rig.send(MessageType::START_REQUEST, addr(3, 1));
    const auto all = rig.poll();
    KRITVA_CHECK(all.size() == 2 && all[0].header.type == MessageType::START_RESPONSE && all[1].header.type == MessageType::FAULT_EVENT);
    KRITVA_CHECK(!decode<LifecycleResponsePayload>(all[0].payload).value.status.ok() && act->lifecycle_state() == LifecycleState::FAULT);
}

static void test_a_fault_with_no_session_is_not_told() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    advance(rig, 300 * kMs);
    rig.poll();                                                                      // the session timed out
    KRITVA_CHECK(rig.imu.acceleration().inject_fault("late").has_value());
    KRITVA_CHECK(rig.poll().empty() && rig.stats().fault_events_sent == 0 && rig.stats().fault_events_dropped == 1);
    // The lost event costs no safety: the Edge endpoint is FAULT and refuses by itself.
    KRITVA_CHECK(rig.imu.acceleration().lifecycle_state() == LifecycleState::FAULT);
}

static void test_a_fault_while_the_session_is_replaced_is_not_told_to_the_new_one() {
    using namespace kritva::hardware;
    EdgeRig rig(false);
    Device bad(DeviceInfo::create(DeviceId{3}, "bad").value());
    auto owned = std::make_unique<kritva::hardware::test::TestActuator>(1, "act");
    kritva::hardware::test::TestActuator* act = owned.get();
    KRITVA_CHECK(bad.add_endpoint(std::move(owned)).has_value() && rig.registry.register_device(bad).has_value());
    KRITVA_CHECK(rig.hello().status.ok());
    KRITVA_CHECK(act->configure(kritva::core::Configuration{}).has_value() && act->initialize().has_value() && act->start().has_value());
    act->fail_at = kritva::hardware::test::FailAt::STOP;
    KRITVA_CHECK(rig.hello().status.ok());                                           // the replacement stops it, and the stop fails
    KRITVA_CHECK(act->lifecycle_state() == LifecycleState::FAULT);
    KRITVA_CHECK(rig.poll().empty() && rig.stats().fault_events_sent == 0 && rig.stats().fault_events_dropped == 1);
}

int main() {
    test_the_edge_sends_one_heartbeat_per_period();
    test_a_jump_in_time_is_not_a_heartbeat_burst();
    test_a_heartbeat_never_acts();
    test_the_timeout_is_exactly_at_the_bound_and_stops_only_actuators();
    test_degraded_is_observation_only_and_a_frame_ends_it();
    test_supervision_runs_only_when_driven();
    test_the_old_session_is_gone_for_good();
    test_a_new_hello_just_before_the_timeout_takes_the_replacement_path();
    test_timeout_stops_every_actuator_and_counts_a_failed_stop();
    test_which_frames_refresh_liveness();
    test_replayed_writes_cannot_keep_an_actuator_alive();
    test_a_fault_outside_a_request_is_told_at_the_next_poll();
    test_a_fault_caused_by_a_request_is_told_after_its_response();
    test_a_failing_lifecycle_request_is_answered_then_the_fault_is_told();
    test_a_fault_with_no_session_is_not_told();
    test_a_fault_while_the_session_is_replaced_is_not_told_to_the_new_one();
    std::printf("edge_supervision_test: PASS\n");
    return 0;
}
