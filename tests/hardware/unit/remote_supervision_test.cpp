//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_supervision_test.cpp
// Description : Unit tests of the Nexus supervision and link-loss policy: heartbeats, DEGRADED, the timeout, the faulting of
//               live remote endpoints with one ERROR each and one link record, FAULT_EVENT, and no automatic recovery.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: FRL-001; FRL-002; FRL-003; SR-003 (HUMAN SAFETY REVIEW OPEN)
// API         : REMOTE-SUPERVISION-UT
//
// Author      : KritvaOS
// Created     : 06-10-2026
//==============================================================================

#include "../support/nexus_rig.hpp"

#include <kritva/hardware/device_manager.hpp>

using namespace kritva::hardware::remote;
using namespace kritva::hardware::remote::test;
using kritva::core::ErrorCode;
using kritva::core::EventType;
using kritva::core::LifecycleState;
namespace hw = kritva::hardware;

static constexpr std::uint64_t kMs = 1'000'000;

static const EndpointAddress kMotorCommand{NodeId{kEdgeNode}, hw::DeviceId{kMotor}, hw::EndpointId{kCommand}};

static void advance(NexusRig& rig, std::uint64_t ns) { KRITVA_CHECK(rig.link().advance(ns).has_value()); }

// Spends virtual time with the sides driven as asked, in steps (supervision runs only when driven).
static void run_for(NexusRig& rig, EdgeHost* edge, std::uint64_t total_ns, bool nexus, bool edge_driven, std::uint64_t step = 10 * kMs) {
    for (std::uint64_t t = 0; t < total_ns; t += step) {
        advance(rig, step);
        if (nexus) rig.node.service();
        if (edge_driven && edge != nullptr) edge->poll();
    }
}

struct Sink final : kritva::core::runtime::IEventSink {
    std::vector<kritva::core::Event> events;
    kritva::core::Result<void> report(const kritva::core::Event& e) override { events.push_back(e); return kritva::core::Result<void>::success(); }
    [[nodiscard]] std::size_t errors() const {
        std::size_t n = 0;
        for (const auto& e : events) n += e.type == EventType::ERROR ? 1 : 0;
        return n;
    }
};

// A Nexus whose four remote endpoints are brought up by the existing DeviceManager, which reports every endpoint fault once.
struct ManagedRig {
    NexusRig rig;
    hw::DeviceManager manager{kritva::core::runtime::ComponentId{300}};
    Sink sink;
    ManagedRig() {
        KRITVA_CHECK(rig.node.connect().has_value());
        for (const auto& d : rig.node.devices()) KRITVA_CHECK(manager.register_device(*d).has_value());
        manager.set_event_sink(&sink);
        KRITVA_CHECK(manager.configure(kritva::core::Configuration{}).has_value() && manager.initialize().has_value() && manager.start().has_value());
    }
    [[nodiscard]] std::size_t faulted() {
        std::size_t n = 0;
        for (const auto& d : rig.node.devices()) for (const hw::Endpoint* e : d->endpoints()) n += e->lifecycle_state() == LifecycleState::FAULT ? 1 : 0;
        return n;
    }
};

// ---- heartbeats --------------------------------------------------------------------------------------------------------

static void test_both_sides_send_a_heartbeat_per_period() {
    NexusRig rig;
    rig.connect_and_run();
    EdgeHost* edge = rig.active_edge;
    const auto edge_sent = edge->stats().heartbeats_sent;
    const auto edge_got = edge->stats().heartbeats;
    const auto sent = rig.session().stats().heartbeats_sent;
    const auto notices = rig.session().stats().notices;
    run_for(rig, edge, 1000 * kMs, true, true);
    rig.node.service();                                                      // the last heartbeat of the Edge was sent in the final step
    KRITVA_CHECK(rig.session().stats().heartbeats_sent - sent == 10 && edge->stats().heartbeats - edge_got == 10);
    KRITVA_CHECK(edge->stats().heartbeats_sent - edge_sent == 10 && rig.session().stats().notices - notices == 10);
    KRITVA_CHECK(rig.session().state() == SessionState::CONNECTED && rig.session().stats().degraded_entries == 0 && rig.session().stats().sessions_lost == 0);
    KRITVA_CHECK(edge->link_state() == SessionState::CONNECTED && rig.node.link_loss_records().empty());
}

static void test_the_first_heartbeat_is_one_period_after_the_session_starts() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    rig.node.service();
    KRITVA_CHECK(rig.session().stats().heartbeats_sent == 0);               // none at the moment of connecting
    advance(rig, 100 * kMs - 1);
    rig.node.service();
    KRITVA_CHECK(rig.session().stats().heartbeats_sent == 0);               // none one nanosecond before the period
    advance(rig, 1);
    rig.node.service();
    KRITVA_CHECK(rig.session().stats().heartbeats_sent == 1);
}

static void test_a_jump_in_time_is_not_a_heartbeat_burst() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    advance(rig, 250 * kMs);
    rig.node.service();
    KRITVA_CHECK(rig.session().stats().heartbeats_sent == 1);
    rig.node.service();
    KRITVA_CHECK(rig.session().stats().heartbeats_sent == 1);
}

// ---- DEGRADED and the timeout --------------------------------------------------------------------------------------------

static void test_degraded_is_observation_only_and_ends_with_a_valid_frame() {
    NexusRig rig;
    rig.connect_and_run();
    EdgeHost* edge = rig.active_edge;
    advance(rig, 199 * kMs);
    rig.node.service();
    KRITVA_CHECK(rig.session().state() == SessionState::CONNECTED);
    advance(rig, 1 * kMs);
    rig.node.service();                                                      // 200 ms without a frame from the Edge: two periods
    KRITVA_CHECK(rig.session().state() == SessionState::DEGRADED && rig.session().stats().degraded_entries == 1);
    for (const auto& d : rig.node.devices()) for (const hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->lifecycle_state() == LifecycleState::RUNNING);   // it never acts
    KRITVA_CHECK(rig.node.link_loss_records().empty());
    edge->poll();                                                            // the Edge is driven again and answers with its heartbeat
    rig.node.service();
    KRITVA_CHECK(rig.session().state() == SessionState::CONNECTED);          // a valid frame returns to CONNECTED
    run_for(rig, edge, 500 * kMs, true, true);
    KRITVA_CHECK(rig.session().state() == SessionState::CONNECTED && rig.session().stats().sessions_lost == 0);
}

static void test_the_timeout_is_exactly_at_the_bound_and_faults_every_live_endpoint_once() {   // FRL-001
    ManagedRig m;
    NexusRig& rig = m.rig;
    KRITVA_CHECK(m.sink.errors() == 0);
    const std::uint64_t old_session = rig.session().session().value();
    advance(rig, 300 * kMs - 1);
    rig.node.service();                                                      // one nanosecond before: still alive (DEGRADED)
    KRITVA_CHECK(rig.session().state() == SessionState::DEGRADED && m.faulted() == 0 && m.sink.errors() == 0 && rig.node.link_loss_records().empty());
    advance(rig, 1);
    rig.node.service();                                                      // exactly the timeout
    KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED && !rig.session().session().valid());
    KRITVA_CHECK(m.faulted() == 4 && m.sink.errors() == 4);                  // N endpoint faults, N ERROR events
    KRITVA_CHECK(rig.node.link_loss_records().size() == 1);                  // and one link-level record
    const LinkLossRecord& rec = rig.node.link_loss_records()[0];
    KRITVA_CHECK(rec.reason == "link lost" && rec.session_id == old_session && rec.endpoints_faulted == 4 && rec.time_ns == rig.link().now_ns());
    for (const auto& d : rig.node.devices()) for (const hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->health().detail() == "link lost");
    // Repeated driving after the loss produces nothing more.
    for (int i = 0; i < 100; ++i) { advance(rig, 10 * kMs); rig.node.service(); }
    KRITVA_CHECK(m.faulted() == 4 && m.sink.errors() == 4 && rig.node.link_loss_records().size() == 1 && rig.session().stats().sessions_lost == 1);
}

static void test_an_already_faulted_endpoint_gets_no_second_event() {
    ManagedRig m;
    NexusRig& rig = m.rig;
    EdgeHost* edge = rig.active_edge;
    KRITVA_CHECK(rig.imu().acceleration().inject_fault("hardware fault").has_value());
    edge->poll();
    rig.node.service();                                                      // the FAULT_EVENT reaches the Nexus
    KRITVA_CHECK(m.faulted() == 1 && m.sink.errors() == 1);
    advance(rig, 300 * kMs);
    rig.node.service();
    KRITVA_CHECK(m.faulted() == 4 && m.sink.errors() == 4);                  // three more, not four
    KRITVA_CHECK(rig.node.link_loss_records()[0].endpoints_faulted == 3);
}

static void test_only_live_endpoints_are_faulted() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    auto& accel = rig.remote<RemoteAccelerationEndpoint>("imu", "acceleration");
    auto& gyro = rig.remote<RemoteAngularVelocityEndpoint>("imu", "angular_velocity");
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    auto& pos = rig.remote<RemotePositionEndpoint>("motor", "position");
    KRITVA_CHECK(accel.initialize().has_value());                            // READY
    KRITVA_CHECK(cmd.initialize().has_value() && cmd.start().has_value());   // RUNNING
    KRITVA_CHECK(gyro.initialize().has_value() && gyro.start().has_value() && gyro.stop().has_value());   // STOPPED
    advance(rig, 300 * kMs);
    rig.node.service();
    KRITVA_CHECK(accel.lifecycle_state() == LifecycleState::FAULT && cmd.lifecycle_state() == LifecycleState::FAULT);
    KRITVA_CHECK(gyro.lifecycle_state() == LifecycleState::STOPPED && pos.lifecycle_state() == LifecycleState::UNKNOWN);   // not live: left alone
    KRITVA_CHECK(rig.node.link_loss_records().size() == 1 && rig.node.link_loss_records()[0].endpoints_faulted == 2);
}

static void test_a_transport_that_goes_down_ends_the_session_at_once() {
    ManagedRig m;
    m.rig.link().disconnect();
    m.rig.node.service();
    KRITVA_CHECK(m.rig.session().state() == SessionState::DISCONNECTED && m.faulted() == 4 && m.sink.errors() == 4);
    KRITVA_CHECK(m.rig.node.link_loss_records().size() == 1 && m.rig.node.link_loss_records()[0].reason == "link lost");
}

static void test_an_explicit_close_faults_the_live_endpoints_with_its_own_reason() {   // approved ruling D
    ManagedRig m;
    NexusRig& rig = m.rig;
    EdgeHost* edge = rig.active_edge;
    const auto edge_session = edge->session().value();
    rig.session().close();
    KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED && m.faulted() == 4 && m.sink.errors() == 4);
    KRITVA_CHECK(rig.node.link_loss_records().size() == 1 && rig.node.link_loss_records()[0].reason == "session closed");
    for (const auto& d : rig.node.devices()) for (const hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->health().detail() == "session closed");
    // Closing is not an Edge safety event: protocol 1.0 has no termination message, so the Edge still has its session and its
    // actuator until its own heartbeat timeout.
    KRITVA_CHECK(edge->session().value() == edge_session && rig.motor().command().lifecycle_state() == LifecycleState::RUNNING);
    rig.session().close();                                                   // closing again is nothing
    KRITVA_CHECK(m.sink.errors() == 4 && rig.node.link_loss_records().size() == 1);
}

static void test_a_failed_handshake_is_not_a_link_loss() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    rig.active_edge = nullptr;                                               // nobody answers a new HELLO
    KRITVA_CHECK(rig.session().reopen().error().code == ErrorCode::TIMEOUT);
    KRITVA_CHECK(rig.node.link_loss_records().empty() && rig.session().stats().sessions_lost == 0);
}

// ---- FAULT_EVENT --------------------------------------------------------------------------------------------------------

static Bytes event_payload(std::uint64_t node, std::uint64_t device, std::uint64_t endpoint, const char* reason) {
    return must(encode(FaultEventPayload{AddressPayload{node, device, endpoint}, LifecycleState::FAULT, reason}));
}

static void test_a_fault_event_faults_the_matching_live_proxy_once() {      // FRL-001, DR-003
    ManagedRig m;
    NexusRig& rig = m.rig;
    const std::uint64_t sid = rig.session().session().value();
    inject_to_nexus(rig.link(), MessageType::FAULT_EVENT, event_payload(kEdgeNode, kImu, kAccel, "boom"), 5000, sid, 0);
    rig.node.service();
    auto& accel = rig.remote<RemoteAccelerationEndpoint>("imu", "acceleration");
    KRITVA_CHECK(accel.lifecycle_state() == LifecycleState::FAULT && accel.health().detail() == "remote fault: boom");
    KRITVA_CHECK(m.sink.errors() == 1 && rig.node.node_stats().remote_faults_applied == 1 && rig.session().stats().fault_events == 1);
    KRITVA_CHECK(rig.session().state() == SessionState::CONNECTED);          // a remote fault is not a link loss
    // The same endpoint again: admitted, but already FAULT: no second transition, no second ERROR.
    inject_to_nexus(rig.link(), MessageType::FAULT_EVENT, event_payload(kEdgeNode, kImu, kAccel, "boom again"), 5001, sid, 0);
    rig.node.service();
    KRITVA_CHECK(m.sink.errors() == 1 && rig.node.node_stats().fault_events_ignored == 1 && accel.health().detail() == "remote fault: boom");
    // A fault of an endpoint that is not live is ignored (STOPPED), as is an unknown address.
    auto& gyro = rig.remote<RemoteAngularVelocityEndpoint>("imu", "angular_velocity");
    KRITVA_CHECK(gyro.stop().has_value());
    inject_to_nexus(rig.link(), MessageType::FAULT_EVENT, event_payload(kEdgeNode, kImu, kGyro, "x"), 5002, sid, 0);
    inject_to_nexus(rig.link(), MessageType::FAULT_EVENT, event_payload(kEdgeNode, 77, 1, "x"), 5003, sid, 0);
    rig.node.service();
    KRITVA_CHECK(gyro.lifecycle_state() == LifecycleState::STOPPED && rig.node.node_stats().fault_events_ignored == 3 && m.sink.errors() == 1);
}

// Two endpoints of one device are both live: an event names exactly one of them.
static void test_a_fault_event_faults_only_the_endpoint_it_names() {
    NexusRig rig;
    rig.connect_and_run();
    const std::uint64_t sid = rig.session().session().value();
    inject_to_nexus(rig.link(), MessageType::FAULT_EVENT, event_payload(kEdgeNode, kImu, kGyro, "gyro broke"), 5000, sid, 0);
    rig.node.service();
    auto& accel = rig.remote<RemoteAccelerationEndpoint>("imu", "acceleration");
    auto& gyro = rig.remote<RemoteAngularVelocityEndpoint>("imu", "angular_velocity");
    KRITVA_CHECK(gyro.lifecycle_state() == LifecycleState::FAULT && accel.lifecycle_state() == LifecycleState::RUNNING);
}

static void test_bad_fault_events_do_nothing() {
    NexusRig rig;
    rig.connect_and_run();
    const std::uint64_t sid = rig.session().session().value();
    const auto& st = rig.session().stats();
    auto all_running = [&] {
        for (const auto& d : rig.node.devices()) for (const hw::Endpoint* e : d->endpoints()) if (e->lifecycle_state() != LifecycleState::RUNNING) return false;
        return true;
    };
    inject_to_nexus(rig.link(), MessageType::FAULT_EVENT, event_payload(99, kImu, kAccel, "x"), 7000, sid, 0);        // another node
    rig.node.service();
    KRITVA_CHECK(st.wrong_node_events == 1 && st.fault_events == 0 && all_running());
    inject_to_nexus(rig.link(), MessageType::FAULT_EVENT, event_payload(kEdgeNode, kImu, kAccel, "x"), 6999, sid, 0);  // stale: below the watermark
    rig.node.service();
    KRITVA_CHECK(st.stale_notices == 1 && all_running());
    inject_to_nexus(rig.link(), MessageType::FAULT_EVENT, Bytes(3, 0), 7001, sid, 0);                                  // malformed payload
    rig.node.service();
    KRITVA_CHECK(st.malformed_notices == 1 && all_running());
    inject_to_nexus(rig.link(), MessageType::FAULT_EVENT, event_payload(kEdgeNode, kImu, kAccel, "x"), 7002, sid + 1, 0);   // another session
    rig.node.service();
    KRITVA_CHECK(st.wrong_session == 1 && all_running());
    KRITVA_CHECK(rig.node.node_stats().remote_faults_applied == 0);
    // The malformed one did not raise the watermark: the next valid one is admitted.
    inject_to_nexus(rig.link(), MessageType::FAULT_EVENT, event_payload(kEdgeNode, kImu, kAccel, "late but valid"), 7001, sid, 0);
    rig.node.service();
    KRITVA_CHECK(rig.node.node_stats().remote_faults_applied == 1);
}

static void test_an_edge_fault_reaches_the_nexus_end_to_end() {
    NexusRig rig;
    rig.connect_and_run();
    EdgeHost* edge = rig.active_edge;
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    KRITVA_CHECK(rig.motor().command().inject_fault("overcurrent").has_value());
    run_for(rig, edge, 20 * kMs, true, true);                                // between calls, both sides driven
    KRITVA_CHECK(cmd.lifecycle_state() == LifecycleState::FAULT && cmd.health().detail() == "remote fault: overcurrent");
    // A lost FAULT_EVENT would cost no safety: the Edge endpoint is FAULT and refuses by itself.
    KRITVA_CHECK(rig.motor().command().lifecycle_state() == LifecycleState::FAULT);
}

// ---- no automatic recovery -------------------------------------------------------------------------------------------------

static void test_nothing_recovers_by_itself_and_the_way_back_is_explicit() {        // FRL-003, approved constraint E7
    ManagedRig m;
    NexusRig& rig = m.rig;
    EdgeHost* edge = rig.active_edge;
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    KRITVA_CHECK(cmd.write(hw::MotorCommand{0.5}).has_value());
    run_for(rig, edge, 400 * kMs, false, true);                              // the Nexus is not driven: the Edge times out and stops the actuator
    KRITVA_CHECK(!edge->session().valid() && rig.motor().command().lifecycle_state() == LifecycleState::STOPPED && rig.motor().command().effective_velocity() == 0.0);
    run_for(rig, edge, 1000 * kMs, true, true);                              // now the Nexus is driven; it too finds nothing alive
    KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED && m.faulted() == 4 && m.sink.errors() == 4);
    const auto sent = rig.session().stats().requests_sent;
    run_for(rig, edge, 2000 * kMs, true, true);                              // two more virtual seconds
    KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED && rig.session().stats().requests_sent == sent);   // no reconnect, nothing sent
    KRITVA_CHECK(m.faulted() == 4 && rig.node.link_loss_records().size() == 1 && !edge->session().valid());
    // A fresh HELLO does not clear a FAULT by itself.
    KRITVA_CHECK(rig.session().reopen().has_value() && m.faulted() == 4);
    rig.session().close();
    // The explicit way: shutdown, initialize, start, on a fresh session.
    for (const auto& d : rig.node.devices()) for (hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->shutdown().has_value() && e->lifecycle_state() == LifecycleState::STOPPED);
    const std::uint64_t before = edge->session().value();
    for (const auto& d : rig.node.devices()) for (hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->initialize().has_value() && e->start().has_value());
    KRITVA_CHECK(rig.session().state() == SessionState::CONNECTED && edge->session().value() > before && m.faulted() == 0);
    KRITVA_CHECK(cmd.write(hw::MotorCommand{0.25}).has_value() && rig.motor().command().effective_velocity() == 0.25);
}

// ---- the Edge protects the actuator whatever the Nexus does ---------------------------------------------------------------

static void test_the_edge_stops_the_actuator_by_itself_when_the_nexus_is_gone() {   // SR-003
    NexusRig rig;
    rig.connect_and_run();
    EdgeHost* edge = rig.active_edge;
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    KRITVA_CHECK(cmd.write(hw::MotorCommand{0.9}).has_value());
    KRITVA_CHECK(rig.motor().command().effective_velocity() == 0.9);
    run_for(rig, edge, 290 * kMs, false, true);
    advance(rig, 9 * kMs);
    edge->poll();
    KRITVA_CHECK(rig.motor().command().effective_velocity() == 0.9);          // 299 ms without a frame: not yet
    advance(rig, 1 * kMs);
    edge->poll();
    KRITVA_CHECK(rig.motor().command().effective_velocity() == 0.0 && rig.motor().command().lifecycle_state() == LifecycleState::STOPPED);
    // The Nexus proxy has not been told and has not acted: that is its own supervision, when it is driven.
    KRITVA_CHECK(cmd.lifecycle_state() == LifecycleState::RUNNING && rig.node.link_loss_records().empty());
    // Sensors kept running on the Edge.
    KRITVA_CHECK(rig.imu().acceleration().lifecycle_state() == LifecycleState::RUNNING && rig.motor().position().lifecycle_state() == LifecycleState::RUNNING);
}

// ---- which frames count on the Nexus ------------------------------------------------------------------------------------------

struct NexusCase {
    const char* what;
    bool refreshes;
    std::function<void(NexusRig&, std::uint64_t sid)> action;
    std::function<void(NexusRig&, std::uint64_t sid)> setup = nullptr;      // optional, at t = 0
};

// The frame arrives at t = 100 ms; the last liveness is at t = 0. The Edge is muted so that only the crafted frame arrives.
static void test_which_frames_refresh_the_nexus_liveness() {
    const Bytes ok_observe = must(encode(ObserveResponsePayload{}));
    const auto muted_observe = [](NexusRig& rig) { return rig.session().observe(kMotorCommand); };
    const std::vector<NexusCase> cases = {
        {"an admitted heartbeat", true, [](NexusRig& r, std::uint64_t sid) {
            inject_to_nexus(r.link(), MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1})), 9000, sid, 0); r.node.service(); }},
        {"an admitted FAULT_EVENT", true, [](NexusRig& r, std::uint64_t sid) {
            inject_to_nexus(r.link(), MessageType::FAULT_EVENT, event_payload(kEdgeNode, 77, 1, "x"), 9000, sid, 0); r.node.service(); }},
        {"a notice PROTOCOL_ERROR with correlation 0", true, [](NexusRig& r, std::uint64_t sid) {
            inject_to_nexus(r.link(), MessageType::PROTOCOL_ERROR, must(encode_protocol_error(StatusPayload{{ErrorCode::INVALID_ARGUMENT, "x"}})), 9000, sid, 0); r.node.service(); }},
        {"a stale heartbeat", false, [](NexusRig& r, std::uint64_t sid) {
            inject_to_nexus(r.link(), MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1})), 8000, sid, 0); r.node.service(); },
         [](NexusRig& r, std::uint64_t sid) {                                // the watermark is raised at t = 0
            inject_to_nexus(r.link(), MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1})), 9000, sid, 0); r.node.service(); }},
        {"a heartbeat of another session", false, [](NexusRig& r, std::uint64_t sid) {
            inject_to_nexus(r.link(), MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1})), 9000, sid + 1, 0); r.node.service(); }},
        {"a heartbeat with a malformed payload", false, [](NexusRig& r, std::uint64_t sid) {
            inject_to_nexus(r.link(), MessageType::HEARTBEAT, Bytes(3, 0), 9000, sid, 0); r.node.service(); }},
        {"a frame with a bad header", false, [](NexusRig& r, std::uint64_t) { r.link().edge().send(Bytes(20, 0xAB)); r.node.service(); }},
        {"a request type that cannot reach a Nexus", false, [](NexusRig& r, std::uint64_t sid) {
            inject_to_nexus(r.link(), MessageType::READ_REQUEST, addr(kImu, kAccel), 9000, sid, 0); r.node.service(); }},
    };
    for (const NexusCase& c : cases) {
        NexusRig rig;
        KRITVA_CHECK(rig.node.connect().has_value());
        const std::uint64_t sid = rig.session().session().value();
        rig.active_edge = nullptr;
        if (c.setup) c.setup(rig, sid);
        advance(rig, 100 * kMs);
        c.action(rig, sid);
        const std::uint64_t expected = c.refreshes ? 100 * kMs : 0;
        if (rig.session().last_valid_frame_ns() != expected) {
            std::fprintf(stderr, "nexus liveness: %s: last valid %llu, expected %llu\n", c.what,
                         static_cast<unsigned long long>(rig.session().last_valid_frame_ns()), static_cast<unsigned long long>(expected));
            std::exit(1);
        }
    }
    // Responses.
    {   // an accepted response (the Edge answers)
        NexusRig rig;
        KRITVA_CHECK(rig.node.connect().has_value());
        advance(rig, 100 * kMs);
        KRITVA_CHECK(rig.session().observe(kMotorCommand).has_value() && rig.session().last_valid_frame_ns() == 100 * kMs);
    }
    struct ResponseCase { const char* what; bool refreshes; std::function<void(NexusRig&, std::uint64_t sid, std::uint64_t seq)> inject; ErrorCode code; };
    const std::vector<ResponseCase> responses = {
        {"a response matching nothing", false, [&](NexusRig& r, std::uint64_t sid, std::uint64_t seq) {
            inject_to_nexus(r.link(), MessageType::OBSERVE_RESPONSE, ok_observe, 500, sid, seq + 50); }, ErrorCode::TIMEOUT},
        {"a response of another session", false, [&](NexusRig& r, std::uint64_t sid, std::uint64_t seq) {
            inject_to_nexus(r.link(), MessageType::OBSERVE_RESPONSE, ok_observe, 500, sid + 1, seq); }, ErrorCode::TIMEOUT},
        {"a matching response of the wrong type", false, [&](NexusRig& r, std::uint64_t sid, std::uint64_t seq) {
            inject_to_nexus(r.link(), MessageType::WRITE_RESPONSE, must(encode(StatusPayload{})), 500, sid, seq); }, ErrorCode::TIMEOUT},
        {"a matching response with a malformed payload", false, [&](NexusRig& r, std::uint64_t sid, std::uint64_t seq) {
            inject_to_nexus(r.link(), MessageType::OBSERVE_RESPONSE, Bytes(3, 0), 500, sid, seq); }, ErrorCode::INVALID_ARGUMENT},
        {"a matching PROTOCOL_ERROR", true, [&](NexusRig& r, std::uint64_t sid, std::uint64_t seq) {
            inject_to_nexus(r.link(), MessageType::PROTOCOL_ERROR, must(encode_protocol_error(StatusPayload{{ErrorCode::INVALID_ARGUMENT, "refused"}})), 500, sid, seq); }, ErrorCode::INVALID_ARGUMENT},
    };
    for (const ResponseCase& c : responses) {
        NexusRig rig;
        KRITVA_CHECK(rig.node.connect().has_value());
        const std::uint64_t sid = rig.session().session().value();
        rig.active_edge = nullptr;
        advance(rig, 100 * kMs);
        const std::uint64_t seq = rig.session().next_request_sequence();
        std::uint64_t at = 0;
        rig.before_edge = [&] { if (at++ == 0) c.inject(rig, sid, seq); };
        const auto r = muted_observe(rig);
        const std::uint64_t expected = c.refreshes ? 100 * kMs : 0;
        if (r.has_value() || r.error().code != c.code || rig.session().last_valid_frame_ns() != expected) {
            std::fprintf(stderr, "nexus liveness (response): %s\n", c.what);
            std::exit(1);
        }
    }
    {   // a late response (the Edge answers after the deadline)
        NexusRig rig;
        KRITVA_CHECK(rig.node.connect().has_value());
        DirectionConfig slow;
        slow.latency_ns = 150 * kMs;
        KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, slow).has_value());
        KRITVA_CHECK(rig.session().observe(kMotorCommand).error().code == ErrorCode::TIMEOUT);
        advance(rig, 60 * kMs);
        rig.node.service();                                                  // the late answer arrives and is dropped
        KRITVA_CHECK(rig.session().stats().late_responses == 1 && rig.session().last_valid_frame_ns() == 0);
    }
}

// ---- determinism -------------------------------------------------------------------------------------------------------------

static std::vector<std::uint64_t> scripted_run(std::uint64_t seed) {
    NexusRig rig;
    rig.connect_and_run();
    EdgeHost* edge = rig.active_edge;
    DirectionConfig c;
    c.latency_ns = 2 * kMs;
    c.seed = seed;
    c.drop_permille = 400;
    c.duplicate_permille = 150;
    c.reorder_permille = 200;
    c.reorder_delay_ns = 7 * kMs;
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, c).has_value());
    c.seed = seed + 1;
    KRITVA_CHECK(rig.link().configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    std::vector<std::uint64_t> log;
    for (int i = 0; i < 40; ++i) {
        run_for(rig, edge, 50 * kMs, true, true);
        log.push_back(static_cast<std::uint64_t>(rig.session().state()));
        log.push_back(rig.node.link_loss_records().size());
        if (rig.session().state() == SessionState::DISCONNECTED) break;
    }
    const auto& s = rig.session().stats();
    for (const auto v : {s.heartbeats_sent, s.notices, s.stale_notices, s.degraded_entries, s.sessions_lost, edge->stats().heartbeats_sent, edge->stats().sessions_timed_out}) log.push_back(v);
    return log;
}

static void test_repeatability() {
    const auto a = scripted_run(3);
    KRITVA_CHECK(a == scripted_run(3) && a.size() > 10);
    bool differs = false;
    for (std::uint64_t seed = 4; seed < 12 && !differs; ++seed) differs = scripted_run(seed) != a;
    KRITVA_CHECK(differs);
}

int main() {
    test_both_sides_send_a_heartbeat_per_period();
    test_the_first_heartbeat_is_one_period_after_the_session_starts();
    test_a_jump_in_time_is_not_a_heartbeat_burst();
    test_degraded_is_observation_only_and_ends_with_a_valid_frame();
    test_the_timeout_is_exactly_at_the_bound_and_faults_every_live_endpoint_once();
    test_an_already_faulted_endpoint_gets_no_second_event();
    test_only_live_endpoints_are_faulted();
    test_a_transport_that_goes_down_ends_the_session_at_once();
    test_an_explicit_close_faults_the_live_endpoints_with_its_own_reason();
    test_a_failed_handshake_is_not_a_link_loss();
    test_a_fault_event_faults_the_matching_live_proxy_once();
    test_a_fault_event_faults_only_the_endpoint_it_names();
    test_bad_fault_events_do_nothing();
    test_an_edge_fault_reaches_the_nexus_end_to_end();
    test_nothing_recovers_by_itself_and_the_way_back_is_explicit();
    test_the_edge_stops_the_actuator_by_itself_when_the_nexus_is_gone();
    test_which_frames_refresh_the_nexus_liveness();
    test_repeatability();
    std::printf("remote_supervision_test: PASS\n");
    return 0;
}
