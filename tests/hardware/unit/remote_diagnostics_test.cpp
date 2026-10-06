//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_diagnostics_test.cpp
// Description : Unit tests of the read-only diagnostics: contents, passive-ness in every state, fault origins and their
//               episodes, the Edge and Nexus views, and the deterministic sanitized text.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DR-001; DR-002; DR-003 (HUMAN SAFETY REVIEW OPEN)
// API         : REMOTE-DIAGNOSTICS-UT
//
// Author      : KritvaOS
// Created     : 06-10-2026
//==============================================================================

#include <sstream>

#include "../support/stack_rig.hpp"

using namespace kritva::hardware::remote;
using namespace kritva::hardware::remote::test;
using kritva::core::ErrorCode;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
namespace hw = kritva::hardware;

static constexpr std::uint64_t kMs = 1'000'000;

static const RemoteEndpointDiagnostics& endpoint_of(const RemoteNodeDiagnostics& d, std::uint64_t device, std::uint64_t endpoint) {
    for (const auto& e : d.endpoints) if (e.address.device.value() == device && e.address.endpoint.value() == endpoint) return e;
    std::fprintf(stderr, "no endpoint %llu.%llu\n", static_cast<unsigned long long>(device), static_cast<unsigned long long>(endpoint));
    std::exit(1);
}

// Everything that a diagnostic call must leave exactly as it was.
struct Fingerprint {
    std::string nexus_text, edge_text;
    std::uint64_t now, next_sequence, last_valid, tx_pending_a, tx_pending_b, sent_a, sent_b, delivered_a, delivered_b, edge_frames, records;
    std::vector<int> states;
    bool operator==(const Fingerprint&) const = default;
};

static Fingerprint fingerprint(StackRig& rig) {
    Fingerprint f;
    f.nexus_text = describe(diagnose(*rig.node));
    f.edge_text = describe(diagnose(rig.edge()));
    f.now = rig.link().now_ns();
    f.next_sequence = rig.session().next_request_sequence();
    f.last_valid = rig.session().last_valid_frame_ns();
    f.tx_pending_a = rig.link().pending(LinkDirection::NEXUS_TO_EDGE);
    f.tx_pending_b = rig.link().pending(LinkDirection::EDGE_TO_NEXUS);
    f.sent_a = rig.link().stats(LinkDirection::NEXUS_TO_EDGE).sent;
    f.sent_b = rig.link().stats(LinkDirection::EDGE_TO_NEXUS).sent;
    f.delivered_a = rig.link().stats(LinkDirection::NEXUS_TO_EDGE).delivered;
    f.delivered_b = rig.link().stats(LinkDirection::EDGE_TO_NEXUS).delivered;
    f.edge_frames = rig.edge().stats().frames_received;
    f.records = rig.node->link_loss_records().size();
    for (const auto& d : rig.node->devices()) for (const hw::Endpoint* e : d->endpoints()) f.states.push_back(static_cast<int>(e->lifecycle_state()));
    for (const auto& d : rig.edge_rig.registry.devices()) for (const hw::Endpoint* e : d->endpoints()) f.states.push_back(static_cast<int>(e->lifecycle_state()));
    return f;
}

// Diagnosing never changes anything, in any state.
static void expect_passive(StackRig& rig, const char* state) {
    const Fingerprint before = fingerprint(rig);
    for (int i = 0; i < 25; ++i) {
        const auto n = diagnose(*rig.node);
        const auto e = diagnose(rig.edge());
        (void)describe(n);
        (void)describe(e);
    }
    if (!(fingerprint(rig) == before)) {
        std::fprintf(stderr, "diagnostics changed something in state %s\n", state);
        std::exit(1);
    }
}

// ---- contents ----------------------------------------------------------------------------------------------------------

static void test_the_nexus_snapshot_has_identity_session_states_and_statistics() {            // DR-001
    StackRig rig;
    rig.up();
    KRITVA_CHECK(rig.remote<RemoteMotorCommandEndpoint>("motor", "command").write(hw::MotorCommand{0.5}).has_value());
    KRITVA_CHECK(rig.loop.advance(7 * kMs).has_value());                                      // a virtual time that is not zero
    const auto d = diagnose(*rig.node);
    KRITVA_CHECK(d.now_ns == 7 * kMs && d.last_valid_frame_ns == rig.session().last_valid_frame_ns());
    KRITVA_CHECK(d.edge_node.value() == kEdgeNode && d.session == rig.session().session() && d.link_state == SessionState::CONNECTED);
    KRITVA_CHECK(d.timing.heartbeat_period_ms == 100 && d.timing.heartbeat_timeout_ms == 300 && d.now_ns == rig.link().now_ns());
    KRITVA_CHECK(d.link_stats.requests_sent == rig.session().stats().requests_sent && d.link_stats.responses_accepted == rig.session().stats().responses_accepted);
    KRITVA_CHECK(d.endpoints.size() == 4 && d.link_loss_records.empty());
    const auto& cmd = endpoint_of(d, kMotor, kCommand);
    KRITVA_CHECK(cmd.address.node.value() == kEdgeNode && cmd.endpoint.device_name == "motor" && cmd.endpoint.endpoint_name == "command");
    KRITVA_CHECK(cmd.endpoint.direction == hw::EndpointDirection::ACTUATOR && cmd.endpoint.lifecycle == LifecycleState::RUNNING &&
                 cmd.endpoint.health == HealthState::HEALTHY && cmd.endpoint.operations_ok == 1 && cmd.endpoint.operations_failed == 0 && !cmd.endpoint.last_error);
    KRITVA_CHECK(cmd.endpoint.capabilities.size() == 1 && cmd.endpoint.capabilities[0] == "motor_command" && cmd.fault_origin == FaultOrigin::NONE);
    // The same I3 snapshot that the DeviceManager gives for the same endpoint.
    const auto managed = rig.manager.diagnostics();
    for (const auto& device : managed) {
        for (const auto& e : device.endpoints) {
            const auto& mine = endpoint_of(d, device.id.value(), e.endpoint_id.value()).endpoint;
            KRITVA_CHECK(mine.lifecycle == e.lifecycle && mine.status == e.status && mine.health == e.health && mine.operations_ok == e.operations_ok &&
                         mine.operations_failed == e.operations_failed && mine.endpoint_name == e.endpoint_name && mine.capabilities == e.capabilities);
        }
    }
}

static void test_a_failed_operation_shows_in_the_snapshot_as_status_and_last_error() {
    StackRig rig;
    rig.up();
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    KRITVA_CHECK(!cmd.write(hw::MotorCommand{9.0}).has_value());                                 // out of the Edge's limits
    const auto d = diagnose(*rig.node);
    const auto& e = endpoint_of(d, kMotor, kCommand);
    KRITVA_CHECK(e.endpoint.operations_failed == 1 && e.endpoint.last_error && e.endpoint.last_error->code == ErrorCode::INVALID_ARGUMENT);
}

static void test_the_edge_snapshot() {
    StackRig rig;
    rig.up();
    const auto d = diagnose(rig.edge());
    KRITVA_CHECK(d.node.value() == kEdgeNode && d.session == rig.edge().session() && d.link_state == SessionState::CONNECTED && d.sealed);
    KRITVA_CHECK(d.timing.heartbeat_period_ms == 100 && d.stats.hellos_accepted >= 1 && d.devices.size() == 2);
    const std::string text = describe(d);
    KRITVA_CHECK(text.find("sealed=true") != std::string::npos && text.find("edge node=7 session=") != std::string::npos);
    StackRig fresh;                                                                           // nothing has said HELLO to this Edge yet? (connect did: it is sealed)
    KRITVA_CHECK(diagnose(fresh.edge()).sealed);
    KRITVA_CHECK(d.devices[1].name == "motor" && d.devices[1].endpoints[0].lifecycle == LifecycleState::RUNNING && d.devices[1].endpoints[0].direction == hw::EndpointDirection::ACTUATOR);
    // It is the real Edge state: changing the endpoint behind the protocol's back shows at once.
    KRITVA_CHECK(rig.motor().command().inject_fault("boom").has_value());
    KRITVA_CHECK(diagnose(rig.edge()).devices[1].endpoints[0].lifecycle == LifecycleState::FAULT);
}

// ---- passive-ness ------------------------------------------------------------------------------------------------------------

static void test_diagnostics_change_nothing_in_any_state() {                                  // approved constraints 6, 12, 13
    StackRig rig;
    expect_passive(rig, "before anything runs");
    rig.up();
    expect_passive(rig, "connected and running");
    KRITVA_CHECK(rig.link().advance(200 * kMs).has_value());                                  // nobody drives: time passes, nothing is evaluated
    expect_passive(rig, "time passed without driving");
    rig.node->service();
    KRITVA_CHECK(rig.session().state() == SessionState::DEGRADED);
    expect_passive(rig, "DEGRADED");
    rig.link().disconnect();
    rig.node->service();
    KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED);
    expect_passive(rig, "DISCONNECTED with endpoints in FAULT");
    KRITVA_CHECK(rig.loop.advance(400 * kMs).has_value());
    expect_passive(rig, "after the Edge timed out too");
    for (const auto& d : rig.node->devices()) for (const hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->lifecycle_state() == LifecycleState::FAULT);   // diagnostics cleared nothing
    KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED);                        // and reconnected nothing
}

static void test_the_returned_copies_cannot_reach_the_node() {                                // approved constraint 5
    StackRig rig;
    rig.up();
    rig.session().close();
    auto d = diagnose(*rig.node);
    KRITVA_CHECK(d.link_loss_records.size() == 1 && rig.node->link_loss_records().size() == 1);
    d.link_loss_records.clear();
    d.link_loss_records.push_back(LinkLossRecord{});
    d.endpoints.clear();
    d.link_stats.timeouts = 12345;
    const auto again = diagnose(*rig.node);
    KRITVA_CHECK(again.link_loss_records.size() == 1 && again.link_loss_records[0].endpoints_faulted == 4 && again.endpoints.size() == 4 && again.link_stats.timeouts == 0);
}

// ---- fault origins ------------------------------------------------------------------------------------------------------------

static void expect_all(const RemoteNodeDiagnostics& d, FaultOrigin origin, const char* detail) {
    for (const auto& e : d.endpoints) {
        if (e.fault_origin != origin || e.endpoint.lifecycle != LifecycleState::FAULT || e.endpoint.health_detail != detail) {
            std::fprintf(stderr, "endpoint %s: origin %s detail '%s'\n", e.endpoint.endpoint_name.c_str(), fault_origin_name(e.fault_origin), e.endpoint.health_detail.c_str());
            std::exit(1);
        }
    }
}

static void test_each_origin_is_recorded_explicitly_with_its_own_reason() {                 // DR-003, approved Q4
    {   // not FAULT: NONE
        StackRig rig;
        for (const auto& e : diagnose(*rig.node).endpoints) KRITVA_CHECK(e.fault_origin == FaultOrigin::NONE);
        rig.up();
        for (const auto& e : diagnose(*rig.node).endpoints) KRITVA_CHECK(e.fault_origin == FaultOrigin::NONE);
    }
    {   // the link was lost
        StackRig rig;
        rig.up();
        rig.link().disconnect();
        rig.node->service();
        const auto d = diagnose(*rig.node);
        expect_all(d, FaultOrigin::LINK_LOST, "link lost");
        KRITVA_CHECK(d.link_loss_records.size() == 1 && d.link_loss_records[0].kind == LinkLossKind::LINK_LOST && d.link_loss_records[0].reason == "link lost");
    }
    {   // the Nexus closed the session
        StackRig rig;
        rig.up();
        rig.session().close();
        const auto d = diagnose(*rig.node);
        expect_all(d, FaultOrigin::SESSION_CLOSED, "session closed");
        KRITVA_CHECK(d.link_loss_records[0].kind == LinkLossKind::SESSION_CLOSED);
        // The Edge is unaware: it still has its session and its running actuator, and times out on its own.
        const auto e = diagnose(rig.edge());
        KRITVA_CHECK(e.session.valid() && e.devices[1].endpoints[0].lifecycle == LifecycleState::RUNNING);
        KRITVA_CHECK(rig.loop.advance(300 * kMs).has_value());
        const auto after = diagnose(rig.edge());
        KRITVA_CHECK(!after.session.valid() && after.devices[1].endpoints[0].lifecycle == LifecycleState::STOPPED && after.stats.sessions_timed_out == 1);
    }
    {   // the Edge told us of a fault of its own
        StackRig rig;
        rig.up();
        KRITVA_CHECK(rig.motor().command().inject_fault("overcurrent").has_value());
        KRITVA_CHECK(rig.loop.advance(5 * kMs).has_value());
        const auto d = diagnose(*rig.node);
        const auto& cmd = endpoint_of(d, kMotor, kCommand);
        KRITVA_CHECK(cmd.fault_origin == FaultOrigin::REMOTE_FAULT && cmd.endpoint.health_detail == "remote fault: overcurrent");
        KRITVA_CHECK(d.node_stats.remote_faults_applied == 1 && d.node_stats.fault_events_ignored == 0 && d.link_stats.fault_events == 1);
        for (const auto& e : d.endpoints) if (&e != &cmd) KRITVA_CHECK(e.fault_origin == FaultOrigin::NONE && e.endpoint.lifecycle == LifecycleState::RUNNING);
        KRITVA_CHECK(d.link_loss_records.empty() && d.link_state == SessionState::CONNECTED);   // a remote fault is not a link loss
        // The Edge's own view, which is the authority for the endpoint, says FAULT as well.
        KRITVA_CHECK(diagnose(rig.edge()).devices[1].endpoints[0].lifecycle == LifecycleState::FAULT);
    }
    {   // the proxy itself failed (a lifecycle request the Edge refused)
        StackRig rig;
        KRITVA_CHECK(rig.motor().command().initialize().has_value());                         // behind the protocol's back: the Edge endpoint is READY already
        auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
        KRITVA_CHECK(!cmd.initialize().has_value());
        const auto d = diagnose(*rig.node);
        const auto& e = endpoint_of(d, kMotor, kCommand);
        KRITVA_CHECK(e.fault_origin == FaultOrigin::LOCAL_FAILURE && e.endpoint.lifecycle == LifecycleState::FAULT && e.endpoint.last_error &&
                     e.endpoint.last_error->code == ErrorCode::INVALID_STATE);
        KRITVA_CHECK(d.link_loss_records.empty());
    }
}

static void test_an_origin_is_fixed_for_its_episode_and_a_new_lifecycle_starts_a_new_one() {   // approved constraint 14
    StackRig rig;
    rig.up();
    KRITVA_CHECK(rig.imu().acceleration().inject_fault("hardware fault").has_value());
    KRITVA_CHECK(rig.loop.advance(5 * kMs).has_value());                                      // accel: REMOTE_FAULT
    rig.link().disconnect();
    rig.node->service();                                                                      // the others: LINK_LOST; accel was FAULT already
    auto d = diagnose(*rig.node);
    KRITVA_CHECK(endpoint_of(d, kImu, kAccel).fault_origin == FaultOrigin::REMOTE_FAULT);
    KRITVA_CHECK(endpoint_of(d, kImu, kGyro).fault_origin == FaultOrigin::LINK_LOST && endpoint_of(d, kMotor, kCommand).fault_origin == FaultOrigin::LINK_LOST);
    KRITVA_CHECK(d.link_loss_records.size() == 1 && d.link_loss_records[0].endpoints_faulted == 3);
    // Neither more driving, nor a close, nor a fresh HELLO changes a recorded origin.
    rig.session().close();
    KRITVA_CHECK(rig.session().reopen().has_value());
    KRITVA_CHECK(rig.loop.advance(500 * kMs).has_value());
    d = diagnose(*rig.node);
    KRITVA_CHECK(endpoint_of(d, kImu, kAccel).fault_origin == FaultOrigin::REMOTE_FAULT && endpoint_of(d, kMotor, kCommand).fault_origin == FaultOrigin::LINK_LOST);
    // The explicit way back ends the episode: shutdown gives NONE, and a later loss is a new episode with its own origin and record.
    rig.session().close();
    for (const auto& dev : rig.node->devices()) for (hw::Endpoint* e : dev->endpoints()) KRITVA_CHECK(e->shutdown().has_value());
    for (const auto& e : diagnose(*rig.node).endpoints) KRITVA_CHECK(e.fault_origin == FaultOrigin::NONE);
    for (const auto& dev : rig.node->devices()) for (hw::Endpoint* e : dev->endpoints()) KRITVA_CHECK(e->initialize().has_value() && e->start().has_value());
    for (const auto& e : diagnose(*rig.node).endpoints) KRITVA_CHECK(e.fault_origin == FaultOrigin::NONE);
    rig.session().close();
    d = diagnose(*rig.node);
    expect_all(d, FaultOrigin::SESSION_CLOSED, "session closed");
    // Three live sessions ended: the loss (3 endpoints faulted), the close after the fresh HELLO (every endpoint was FAULT already: 0),
    // and this one (4).
    KRITVA_CHECK(d.link_loss_records.size() == 3 && d.link_loss_records[0].endpoints_faulted == 3 && d.link_loss_records[1].endpoints_faulted == 0 &&
                 d.link_loss_records[2].kind == LinkLossKind::SESSION_CLOSED && d.link_loss_records[2].endpoints_faulted == 4);
    // A proxy that failed by itself keeps LOCAL_FAILURE through a later link loss.
    StackRig other;
    KRITVA_CHECK(other.motor().command().initialize().has_value());
    KRITVA_CHECK(!other.remote<RemoteMotorCommandEndpoint>("motor", "command").initialize().has_value());
    auto& accel = other.remote<RemoteAccelerationEndpoint>("imu", "acceleration");
    KRITVA_CHECK(accel.initialize().has_value());
    other.session().close();
    const auto od = diagnose(*other.node);
    KRITVA_CHECK(endpoint_of(od, kMotor, kCommand).fault_origin == FaultOrigin::LOCAL_FAILURE && endpoint_of(od, kImu, kAccel).fault_origin == FaultOrigin::SESSION_CLOSED);
}

// A new lifecycle is a new fault episode: a local failure after an earlier link loss is not reported as that link loss.
static void test_a_new_episode_does_not_inherit_the_previous_origin() {
    StackRig rig;
    rig.up();
    rig.link().disconnect();
    rig.node->service();                                                                      // episode 1: LINK_LOST
    KRITVA_CHECK(endpoint_of(diagnose(*rig.node), kMotor, kCommand).fault_origin == FaultOrigin::LINK_LOST);
    KRITVA_CHECK(rig.loop.advance(400 * kMs).has_value());                                    // the Edge times out and stops its actuator (STOPPED)
    for (const auto& dev : rig.node->devices()) for (hw::Endpoint* e : dev->endpoints()) KRITVA_CHECK(e->shutdown().has_value());
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    KRITVA_CHECK(cmd.initialize().has_value());                                               // a fresh session; the cleanup of the I4-005 recovery runs first
    KRITVA_CHECK(rig.motor().command().stop().has_value());                                   // the Edge endpoint is stopped behind the proxy's back ...
    KRITVA_CHECK(!cmd.start().has_value());                                                   // ... so its START is refused: episode 2, the proxy's own failure
    KRITVA_CHECK(endpoint_of(diagnose(*rig.node), kMotor, kCommand).fault_origin == FaultOrigin::LOCAL_FAILURE);
}

// The proxy itself never changes the origin of an endpoint that is FAULT already.
static void test_the_proxy_keeps_the_origin_of_a_fault_that_exists() {
    SimulatedTransport link;
    RemoteSessionConfig cfg;
    cfg.nexus_node = NodeId{kNexusNode};
    cfg.edge_node = NodeId{kEdgeNode};
    RemoteNode node(link.nexus(), cfg);
    RemoteProxy proxy(node, EndpointAddress{NodeId{kEdgeNode}, hw::DeviceId{1}, hw::EndpointId{1}}, {});
    LifecycleState state = LifecycleState::RUNNING;
    int faulted = 0;
    proxy.bind([&] { return state; }, [&](const kritva::core::Error&) { ++faulted; state = LifecycleState::FAULT; });
    const kritva::core::Error cause{ErrorCode::RESOURCE_UNAVAILABLE, kritva::core::ErrorSeverity::ERROR, {}, {}, "x"};
    KRITVA_CHECK(proxy.origin() == FaultOrigin::NONE);                                        // not FAULT
    proxy.fault(cause, FaultOrigin::REMOTE_FAULT);                                            // live: becomes FAULT with this origin
    KRITVA_CHECK(faulted == 1 && proxy.origin() == FaultOrigin::REMOTE_FAULT);
    proxy.fault(cause, FaultOrigin::LINK_LOST);                                               // already FAULT: nothing, and the origin is not overwritten
    KRITVA_CHECK(faulted == 1 && proxy.origin() == FaultOrigin::REMOTE_FAULT);
    LifecycleState stopped = LifecycleState::STOPPED;
    RemoteProxy other(node, EndpointAddress{NodeId{kEdgeNode}, hw::DeviceId{1}, hw::EndpointId{2}}, {});
    other.bind([&] { return stopped; }, [&](const kritva::core::Error&) { ++faulted; });
    other.fault(cause, FaultOrigin::LINK_LOST);                                               // not live: left alone
    KRITVA_CHECK(faulted == 1 && other.origin() == FaultOrigin::NONE);
}

// ---- Edge authority versus the Nexus representation ---------------------------------------------------------------------------

static void test_the_two_views_may_legitimately_differ_until_the_supervision_is_driven() {  // approved constraint 8
    StackRig rig;
    rig.up();
    KRITVA_CHECK(rig.remote<RemoteMotorCommandEndpoint>("motor", "command").write(hw::MotorCommand{0.5}).has_value());
    // Only the Edge is driven past its timeout (the Nexus is not): no hidden synchronization makes the views agree.
    KRITVA_CHECK(rig.link().advance(300 * kMs).has_value());
    rig.edge().poll();
    const auto edge = diagnose(rig.edge());
    const auto nexus = diagnose(*rig.node);
    KRITVA_CHECK(!edge.session.valid() && edge.link_state == SessionState::DISCONNECTED && edge.devices[1].endpoints[0].lifecycle == LifecycleState::STOPPED);
    KRITVA_CHECK(rig.motor().command().effective_velocity() == 0.0);                           // the actuator is safe
    KRITVA_CHECK(nexus.link_state == SessionState::CONNECTED && endpoint_of(nexus, kMotor, kCommand).endpoint.lifecycle == LifecycleState::RUNNING &&
                 endpoint_of(nexus, kMotor, kCommand).fault_origin == FaultOrigin::NONE);       // the Nexus does not know yet
    // Driving the Nexus's supervision (explicitly) is what brings its representation in line.
    rig.node->service();
    const auto later = diagnose(*rig.node);
    KRITVA_CHECK(later.link_state == SessionState::DISCONNECTED);
    expect_all(later, FaultOrigin::LINK_LOST, "link lost");
}

// ---- text -------------------------------------------------------------------------------------------------------------------------

static void test_the_text_is_deterministic_and_keeps_the_three_reasons_verbatim() {          // DR-003
    StackRig rig;
    rig.up();
    KRITVA_CHECK(rig.imu().acceleration().inject_fault("hardware fault").has_value());
    KRITVA_CHECK(rig.loop.advance(5 * kMs).has_value());
    const auto remote_text = describe(diagnose(*rig.node));
    KRITVA_CHECK(remote_text == describe(diagnose(*rig.node)));
    KRITVA_CHECK(remote_text.find("remote fault: hardware fault") != std::string::npos && remote_text.find("fault_origin=REMOTE_FAULT") != std::string::npos);
    rig.link().disconnect();
    rig.node->service();
    const auto lost = describe(diagnose(*rig.node));
    KRITVA_CHECK(lost.find("detail=\"link lost\"") != std::string::npos && lost.find("kind=LINK_LOST reason=\"link lost\"") != std::string::npos && lost.find("fault_origin=LINK_LOST") != std::string::npos);
    StackRig closed;
    closed.up();
    closed.session().close();
    const auto text = describe(diagnose(*closed.node));
    KRITVA_CHECK(text.find("detail=\"session closed\"") != std::string::npos && text.find("kind=SESSION_CLOSED reason=\"session closed\"") != std::string::npos);
    // The generic words that would hide the cause do not appear.
    KRITVA_CHECK(lost.find("remote error") == std::string::npos);
}

static void test_the_text_is_sanitized_and_has_no_configuration_values() {                   // DR-002
    RemoteNodeDiagnostics d;
    d.edge_node = NodeId{7};
    d.link_loss_records.push_back(LinkLossRecord{5, LinkLossKind::LINK_LOST, "a\nfake line=1 \"quoted\"\x01", 1, 2});
    RemoteEndpointDiagnostics e;
    e.address = EndpointAddress{NodeId{7}, hw::DeviceId{1}, hw::EndpointId{1}};
    e.endpoint.device_id = hw::DeviceId{1};
    e.endpoint.device_name = "dev\nfake";
    e.endpoint.endpoint_id = hw::EndpointId{1};
    e.endpoint.endpoint_name = "ep\"x";
    e.endpoint.lifecycle = LifecycleState::FAULT;
    e.endpoint.health_detail = "bad\r\ndetail=\"x\"";
    e.endpoint.last_error = kritva::core::Error{ErrorCode::INTERNAL_ERROR, kritva::core::ErrorSeverity::ERROR, {}, {}, "err\nmore\x7f\""};
    e.endpoint.capabilities = {"cap one", "cap,two"};
    d.endpoints.push_back(e);
    const std::string text = describe(d);
    std::istringstream lines(text);
    for (std::string line; std::getline(lines, line);) {
        KRITVA_CHECK(line.rfind("fake", 0) != 0 && line.rfind("detail=", 0) != 0 && line.rfind("more", 0) != 0);   // no forged line
        std::size_t quotes = 0;
        for (const char c : line) quotes += c == '"' ? 1 : 0;
        KRITVA_CHECK(quotes % 2 == 0);                                                          // no quoted field was opened or closed from outside
        for (const char c : line) KRITVA_CHECK(static_cast<unsigned char>(c) >= 0x20 && c != 0x7f);
    }
    KRITVA_CHECK(text.find("a?fake line=1 ?quoted??") != std::string::npos);
    // The Edge text sanitizes the names of a hand-made snapshot as well.
    EdgeDiagnostics edge;
    hw::DeviceDiagnostics dev;
    dev.id = hw::DeviceId{1};
    dev.name = "dev\nfake edge";
    hw::EndpointDiagnostics ep;
    ep.device_id = dev.id;
    ep.device_name = dev.name;
    ep.endpoint_id = hw::EndpointId{1};
    ep.endpoint_name = "ep\"x\nforged";
    dev.endpoints.push_back(ep);
    edge.devices.push_back(dev);
    std::istringstream edge_lines(describe(edge));
    for (std::string line; std::getline(edge_lines, line);) {
        KRITVA_CHECK(line.rfind("fake", 0) != 0 && line.rfind("forged", 0) != 0);
        std::size_t quotes = 0;
        for (const char c : line) quotes += c == '"' ? 1 : 0;
        KRITVA_CHECK(quotes % 2 == 0);
    }
    // No configuration value reaches the text, in either snapshot.
    StackRig rig("runtime.name=secret_test\nmotor.command.fail_after_writes=424242\nimu.acceleration.start=313131\n");
    rig.up();
    KRITVA_CHECK(rig.remote<RemoteMotorCommandEndpoint>("motor", "command").write(hw::MotorCommand{0.5}).has_value());
    for (const std::string& t : {describe(diagnose(*rig.node)), describe(diagnose(rig.edge()))}) {
        KRITVA_CHECK(t.find("424242") == std::string::npos && t.find("313131") == std::string::npos && t.find("secret_test") == std::string::npos);
    }
}

int main() {
    test_the_nexus_snapshot_has_identity_session_states_and_statistics();
    test_a_failed_operation_shows_in_the_snapshot_as_status_and_last_error();
    test_the_edge_snapshot();
    test_diagnostics_change_nothing_in_any_state();
    test_the_returned_copies_cannot_reach_the_node();
    test_each_origin_is_recorded_explicitly_with_its_own_reason();
    test_an_origin_is_fixed_for_its_episode_and_a_new_lifecycle_starts_a_new_one();
    test_a_new_episode_does_not_inherit_the_previous_origin();
    test_the_proxy_keeps_the_origin_of_a_fault_that_exists();
    test_the_two_views_may_legitimately_differ_until_the_supervision_is_driven();
    test_the_text_is_deterministic_and_keeps_the_three_reasons_verbatim();
    test_the_text_is_sanitized_and_has_no_configuration_values();
    std::printf("remote_diagnostics_test: PASS\n");
    return 0;
}
