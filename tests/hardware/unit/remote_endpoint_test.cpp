//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_endpoint_test.cpp
// Description : Unit tests of the Nexus-side RemoteDevice and RemoteEndpoint proxies: unchanged I3 contracts, live
//               period and fresh session, topology equivalence, Edge-authoritative writes, timeout and
//               retransmission, link loss and explicit recovery, and use inside the existing DeviceManager.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: RR-001; RR-002; RR-003; NDR-003; SR-001 (HUMAN SAFETY REVIEW OPEN)
// API         : REMOTE-ENDPOINT-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <cmath>

#include "../support/nexus_rig.hpp"

#include <kritva/hardware/device_manager.hpp>

using namespace kritva::hardware::remote;
using namespace kritva::hardware::remote::test;
using kritva::core::ErrorCode;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
namespace hw = kritva::hardware;

static constexpr std::uint64_t kMs = 1'000'000;

static kritva::core::Configuration config(std::initializer_list<std::pair<const char*, std::int64_t>> values) {
    kritva::core::Configuration c;
    for (const auto& [k, v] : values) KRITVA_CHECK(c.set(kritva::core::Parameter{k, v, {}}).has_value());
    return c;
}

static void test_connect_builds_ordinary_typed_devices() {                 // RR-001, RR-003
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    KRITVA_CHECK(rig.node.devices().size() == 2);
    const hw::Device& imu = *rig.node.devices()[0];
    const hw::Device& motor = *rig.node.devices()[1];
    KRITVA_CHECK(imu.info().id().value() == kImu && imu.info().name() == "imu" && motor.info().id().value() == kMotor && motor.info().name() == "motor");
    KRITVA_CHECK(imu.endpoints().size() == 2 && motor.endpoints().size() == 2);
    // The application sees the unchanged I3 typed endpoints.
    KRITVA_CHECK(dynamic_cast<hw::AccelerationEndpoint*>(imu.endpoints()[0]) != nullptr);
    KRITVA_CHECK(dynamic_cast<hw::AngularVelocityEndpoint*>(imu.endpoints()[1]) != nullptr);
    KRITVA_CHECK(dynamic_cast<hw::MotorCommandEndpoint*>(motor.endpoints()[0]) != nullptr);
    KRITVA_CHECK(dynamic_cast<hw::PositionEndpoint*>(motor.endpoints()[1]) != nullptr);
    KRITVA_CHECK(dynamic_cast<hw::MotorCommandEndpoint*>(imu.endpoints()[0]) == nullptr);
    // Identity, direction and capability follow the discovery.
    const hw::Endpoint& cmd = *motor.endpoints()[0];
    KRITVA_CHECK(cmd.info().id().value() == kCommand && cmd.info().name() == "command" && cmd.info().direction() == hw::EndpointDirection::ACTUATOR);
    KRITVA_CHECK(cmd.capabilities().size() == 1 && cmd.capabilities().all()[0].id.value() == 0x2001);
    KRITVA_CHECK(imu.endpoints()[0]->capabilities().all()[0].id.value() == 0x1001 && imu.endpoints()[0]->info().direction() == hw::EndpointDirection::SENSOR);
    for (const auto& d : rig.node.devices()) for (const hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->lifecycle_state() == LifecycleState::UNKNOWN);
    // Declared remote settings are the endpoint's setting names; others have none.
    KRITVA_CHECK(cmd.setting_names().size() == 4 && cmd.setting_names()[0] == "min_rad_s");
    KRITVA_CHECK(motor.endpoints()[1]->setting_names().empty());
    // Not a discovery of the Edge's own devices: the Nexus objects are separate objects.
    KRITVA_CHECK(static_cast<const hw::Endpoint*>(&rig.motor().command()) != &cmd);
}

static void test_lifecycle_maps_to_requests_and_a_fresh_session_per_live_period() {   // NDR-003
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    auto& pos = rig.remote<RemotePositionEndpoint>("motor", "position");
    KRITVA_CHECK(rig.edge().session().value() == 1 && rig.node.live_endpoints() == 0);

    KRITVA_CHECK(cmd.configure(config({{"min_rad_s", -5}, {"max_rad_s", 5}})).has_value());   // scoped, bare names, sent as settings
    KRITVA_CHECK(rig.motor().command().limits().min_rad_s == -5.0 && rig.motor().command().limits().max_rad_s == 5.0);
    KRITVA_CHECK(cmd.lifecycle_state() == LifecycleState::STOPPED || cmd.lifecycle_state() == LifecycleState::UNKNOWN);

    KRITVA_CHECK(cmd.initialize().has_value());                             // the first initialize of a live period: a FRESH session
    KRITVA_CHECK(rig.edge().session().value() == 2 && rig.session().session().value() == 2 && rig.node.live_endpoints() == 1);
    KRITVA_CHECK(cmd.lifecycle_state() == LifecycleState::READY && rig.motor().command().lifecycle_state() == LifecycleState::READY);
    KRITVA_CHECK(pos.initialize().has_value());                             // the next one shares it
    KRITVA_CHECK(rig.edge().session().value() == 2 && rig.node.live_endpoints() == 2 && rig.motor().position().lifecycle_state() == LifecycleState::READY);
    KRITVA_CHECK(cmd.start().has_value() && rig.motor().command().lifecycle_state() == LifecycleState::RUNNING && cmd.lifecycle_state() == LifecycleState::RUNNING);
    KRITVA_CHECK(cmd.stop().has_value() && rig.motor().command().lifecycle_state() == LifecycleState::STOPPED);
    KRITVA_CHECK(cmd.shutdown().has_value() && rig.node.live_endpoints() == 1);
    KRITVA_CHECK(pos.start().has_value() && pos.stop().has_value() && pos.shutdown().has_value() && rig.node.live_endpoints() == 0);
    // The live period is over: the next initialize is a new session again.
    KRITVA_CHECK(cmd.initialize().has_value() && rig.edge().session().value() == 3);
}

static void test_the_i3_state_machine_is_the_endpoints_own() {            // RR-003
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    auto& accel = rig.remote<RemoteAccelerationEndpoint>("imu", "acceleration");
    kritva::hardware::AccelerationSample s;
    const auto sent = rig.session().stats().requests_sent;
    const auto before = accel.read(s);                                       // not running: the I3 check answers locally
    KRITVA_CHECK(!before.has_value() && before.error().code == ErrorCode::NOT_READY && rig.session().stats().requests_sent == sent);
    KRITVA_CHECK(accel.start().error().code == ErrorCode::INVALID_STATE && rig.session().stats().requests_sent == sent);   // an invalid transition is refused locally
    KRITVA_CHECK(accel.initialize().has_value() && accel.start().has_value());
    KRITVA_CHECK(accel.read(s).has_value());
    KRITVA_CHECK(accel.stop().has_value() && accel.read(s).error().code == ErrorCode::NOT_READY);
    KRITVA_CHECK(accel.statistics().sample_count.value() >= 1 && accel.statistics().error_count.value() >= 1);       // the endpoint's own counters
}

static void test_typed_reads() {                                           // RR-001
    NexusRig rig;
    rig.connect_and_run();
    auto& accel = rig.remote<RemoteAccelerationEndpoint>("imu", "acceleration");
    auto& gyro = rig.remote<RemoteAngularVelocityEndpoint>("imu", "angular_velocity");
    auto& pos = rig.remote<RemotePositionEndpoint>("motor", "position");
    hw::AccelerationSample a1, a2;
    KRITVA_CHECK(accel.read(a1).has_value() && accel.read(a2).has_value());
    KRITVA_CHECK(a1.sequence == 1 && a2.sequence == 2 && a1.timestamp.domain() == kritva::core::ClockDomain::MONOTONIC && a2.timestamp.nanoseconds() > a1.timestamp.nanoseconds());
    KRITVA_CHECK(hw::is_valid(a1) && hw::is_valid(a2));
    KRITVA_CHECK(a1.value.z == 9.81 && a1.value.y == -a1.value.x && a2.value.z == 9.81 && a2.value.y == -a2.value.x);   // the mock's own shape of sample
    // The remote sample is the Edge endpoint's: the next sample in its numbering is 3.
    hw::AccelerationSample direct;
    KRITVA_CHECK(rig.imu().acceleration().read(direct).has_value() && direct.sequence == 3);
    hw::AccelerationSample a4;
    KRITVA_CHECK(accel.read(a4).has_value() && a4.sequence == 4);
    hw::AngularVelocitySample g;
    KRITVA_CHECK(gyro.read(g).has_value() && g.sequence == 1 && hw::is_valid(g));
    hw::PositionSample p;
    KRITVA_CHECK(pos.read(p).has_value() && p.sequence == 1 && std::isfinite(p.value));
    KRITVA_CHECK(p.value == rig.motor().model().position_rad);              // the value the Edge endpoint produced
    KRITVA_CHECK(g.value.y == 0.0 && g.value.z == 0.0);
    // Counted by the Nexus endpoint like any I3 read.
    KRITVA_CHECK(accel.statistics().sample_count.value() == 3 && accel.statistics().error_count.value() == 0);
}

static void test_a_failing_remote_read_returns_the_edge_error_unchanged() {
    NexusRig rig;
    rig.connect_and_run();
    auto& accel = rig.remote<RemoteAccelerationEndpoint>("imu", "acceleration");
    rig.imu().acceleration().fail_next_operation();
    hw::AccelerationSample s;
    const auto r = accel.read(s);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INTERNAL_ERROR && r.error().message == "injected read failure");
    KRITVA_CHECK(accel.last_error() && accel.last_error()->message == "injected read failure" && accel.statistics().error_count.value() == 1);
    KRITVA_CHECK(accel.lifecycle_state() == LifecycleState::RUNNING && accel.read(s).has_value());     // a failed read does not fault
    // An Edge endpoint that is faulted answers RESOURCE_UNAVAILABLE, and tells the Nexus with a FAULT_EVENT (sent after the
    // answer): the proxy goes FAULT with a reason that says the fault is the Edge's.
    KRITVA_CHECK(rig.imu().acceleration().inject_fault("hardware fault").has_value());
    const auto faulted = accel.read(s);
    KRITVA_CHECK(!faulted.has_value() && faulted.error().code == ErrorCode::RESOURCE_UNAVAILABLE);
    KRITVA_CHECK(accel.lifecycle_state() == LifecycleState::FAULT && accel.health().detail() == "remote fault: hardware fault");   // the reason is the health detail
}

// ---- the Edge stays authoritative ------------------------------------------------------------------------------------

static void test_writes_are_validated_by_the_edge_not_the_nexus() {          // SR-001
    NexusRig rig;
    rig.connect_and_run();
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    KRITVA_CHECK(cmd.write(hw::MotorCommand{0.5}).has_value() && rig.motor().model().velocity_rad_s == 0.5);
    // The Nexus knows no limit: an out-of-limit command IS sent, and the Edge's own endpoint refuses it with its own error.
    const auto sent = rig.session().stats().requests_sent;
    const auto direct = rig.motor().command().write(hw::MotorCommand{5.0});                      // what the Edge endpoint itself says
    KRITVA_CHECK(!direct.has_value());
    const auto r = cmd.write(hw::MotorCommand{5.0});
    KRITVA_CHECK(!r.has_value() && r.error().code == direct.error().code && r.error().message == direct.error().message);
    KRITVA_CHECK(rig.session().stats().requests_sent == sent + 1 && rig.edge().stats().writes_rejected == 1);
    KRITVA_CHECK(rig.motor().model().velocity_rad_s == 0.5 && cmd.statistics().error_count.value() == 1);
    // Only encodability is checked locally: a non-finite value is never sent.
    const auto nan = cmd.write(hw::MotorCommand{std::nan("")});
    KRITVA_CHECK(!nan.has_value() && rig.session().stats().requests_sent == sent + 1);
    KRITVA_CHECK(cmd.write(hw::MotorCommand{INFINITY}).has_value() == false && rig.session().stats().requests_sent == sent + 1);
    // The Edge's state check applies: a stopped Edge endpoint refuses (the Nexus endpoint is RUNNING, the Edge one is not).
    KRITVA_CHECK(rig.motor().command().stop().has_value());
    const auto stopped = cmd.write(hw::MotorCommand{0.1});
    KRITVA_CHECK(!stopped.has_value() && stopped.error().code == ErrorCode::NOT_READY);
    // Bring the Edge endpoint back (the test stopped it behind the proxy's back), so that both sides agree again.
    KRITVA_CHECK(rig.motor().command().initialize().has_value() && rig.motor().command().start().has_value());
    // Limits are the Edge's configuration: tighten them there (through the remote configure path) and the next run obeys them.
    KRITVA_CHECK(cmd.stop().has_value() && cmd.shutdown().has_value());
    KRITVA_CHECK(cmd.configure(config({{"max_rad_s", 0}, {"min_rad_s", 0}})).has_value() && cmd.initialize().has_value() && cmd.start().has_value());
    KRITVA_CHECK(cmd.write(hw::MotorCommand{0.1}).error().code == ErrorCode::INVALID_ARGUMENT && cmd.write(hw::MotorCommand{0.0}).has_value());
}

static void test_a_faulted_edge_actuator_faults_the_proxy_through_a_fault_event() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    KRITVA_CHECK(cmd.configure(config({{"fault_after_writes", 1}})).has_value() && cmd.initialize().has_value() && cmd.start().has_value());
    KRITVA_CHECK(cmd.write(hw::MotorCommand{0.5}).has_value());                                   // succeeds, then the Edge endpoint faults itself
    KRITVA_CHECK(rig.motor().command().lifecycle_state() == LifecycleState::FAULT);
    KRITVA_CHECK(rig.edge().stats().fault_events_sent == 1);                                       // told after the response, once
    KRITVA_CHECK(cmd.lifecycle_state() == LifecycleState::FAULT && cmd.health().detail().rfind("remote fault: ", 0) == 0);
    KRITVA_CHECK(rig.node.node_stats().remote_faults_applied == 1);
    const auto sent = rig.session().stats().requests_sent;
    const auto r = cmd.write(hw::MotorCommand{0.1});                                               // a FAULT endpoint refuses locally (I3), nothing is sent
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::RESOURCE_UNAVAILABLE && rig.session().stats().requests_sent == sent);
}

static void test_a_timed_out_write_has_an_unknown_outcome_and_retransmission_is_explicit() {
    NexusRig rig;
    rig.connect_and_run();
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    DirectionConfig lossy;
    lossy.faults[0].drop = true;
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, lossy).has_value());
    const auto t0 = rig.link().now_ns();
    const auto r = cmd.write(hw::MotorCommand{0.5});
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::TIMEOUT && r.error().message.find("outcome on the Edge is unknown") != std::string::npos);
    KRITVA_CHECK(rig.link().now_ns() - t0 == 100 * kMs);
    KRITVA_CHECK(rig.motor().model().velocity_rad_s == 0.5);                                      // applied although the Nexus never learned it
    KRITVA_CHECK(cmd.lifecycle_state() == LifecycleState::RUNNING && cmd.statistics().error_count.value() == 1);
    // No automatic retransmission happened.
    KRITVA_CHECK(rig.session().stats().retransmissions == 0 && rig.edge().stats().writes_applied == 1 && rig.edge().stats().writes_resent == 0);

    const std::uint64_t ops = rig.motor().command().statistics().sample_count.value();
    KRITVA_CHECK(cmd.retransmit_last_write().has_value());                                        // explicit, same sequence: answered from the ledger
    KRITVA_CHECK(rig.edge().stats().writes_resent == 1 && rig.edge().stats().writes_applied == 1 && rig.motor().command().statistics().sample_count.value() == ops);
    KRITVA_CHECK(cmd.statistics().sample_count.value() >= 1);                                     // counted like a write
    // It needs the endpoint to be running, like a write.
    KRITVA_CHECK(cmd.stop().has_value() && cmd.retransmit_last_write().error().code == ErrorCode::NOT_READY);
}

// ---- topology -----------------------------------------------------------------------------------------------------------

struct SecondEdge {
    hw::DeviceRegistry registry;
    std::unique_ptr<EdgeHost> host;
    SecondEdge(NexusRig& rig, std::initializer_list<hw::Device*> devices) {
        for (hw::Device* d : devices) KRITVA_CHECK(registry.register_device(*d).has_value());
        auto created = EdgeHost::create(NodeId{kEdgeNode}, registry, rig.link().edge());
        KRITVA_CHECK(created.has_value());
        host = std::move(created.value());
    }
};

static void test_initialize_compares_the_topology_as_a_set() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    {   // the same devices in another order: equivalent, initialize succeeds
        SecondEdge reordered(rig, {&rig.motor(), &rig.imu()});
        rig.active_edge = reordered.host.get();
        KRITVA_CHECK(cmd.initialize().has_value() && rig.session().state() == SessionState::CONNECTED && reordered.host->session().value() == 1);
        KRITVA_CHECK(cmd.stop().has_value() && cmd.shutdown().has_value());      // I3: shutdown only from STOPPED, UNKNOWN or FAULT
    }
    {   // a different topology: a deterministic failure, the session is DISCONNECTED and the endpoint FAULT
        SecondEdge changed(rig, {&rig.imu()});
        rig.active_edge = changed.host.get();
        const auto r = cmd.initialize();
        KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::CONFIGURATION_ERROR);
        KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED && cmd.lifecycle_state() == LifecycleState::FAULT && rig.node.live_endpoints() == 0);
        KRITVA_CHECK(cmd.shutdown().has_value() && cmd.lifecycle_state() == LifecycleState::STOPPED);   // the way back is shutdown, initialize, start
        rig.active_edge = rig.edge_rig.edge.get();                                                       // the real Edge is back
        KRITVA_CHECK(cmd.initialize().has_value() && cmd.start().has_value() && cmd.write(hw::MotorCommand{0.2}).has_value());
    }
}

// ---- link loss and explicit recovery (the policy itself is I4-006) ---------------------------------------------------

static void test_link_loss_faults_the_live_proxies_and_recovery_is_explicit() {        // FRL-001, FRL-003
    NexusRig rig;
    rig.connect_and_run();
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    auto& accel = rig.remote<RemoteAccelerationEndpoint>("imu", "acceleration");
    KRITVA_CHECK(cmd.write(hw::MotorCommand{0.5}).has_value());
    rig.link().disconnect();
    hw::AccelerationSample s;
    const auto r = accel.read(s);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::RESOURCE_UNAVAILABLE);
    KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED);
    // The Nexus side: every live proxy is FAULT with the deterministic reason, and ONE link record says so.
    for (const auto& d : rig.node.devices()) {
        for (hw::Endpoint* e : d->endpoints()) {
            KRITVA_CHECK(e->lifecycle_state() == LifecycleState::FAULT && e->health().detail() == "link lost");
        }
    }
    KRITVA_CHECK(rig.node.link_loss_records().size() == 1 && rig.node.link_loss_records()[0].endpoints_faulted == 4 && rig.node.link_loss_records()[0].reason == "link lost");
    KRITVA_CHECK(cmd.write(hw::MotorCommand{0.1}).error().code == ErrorCode::RESOURCE_UNAVAILABLE);      // refused locally: FAULT
    // The Edge was not told (the link is down); it stops its actuator by its own timeout, when it is driven.
    KRITVA_CHECK(rig.motor().command().lifecycle_state() == LifecycleState::RUNNING);
    KRITVA_CHECK(rig.link().advance(300 * kMs).has_value() && rig.edge().poll() == 0);
    KRITVA_CHECK(rig.motor().command().lifecycle_state() == LifecycleState::STOPPED && rig.edge().stats().sessions_timed_out == 1);

    // The explicit way back: shutdown (local when nobody is reachable), initialize (a fresh HELLO), start. A fresh HELLO does
    // not clear a FAULT by itself.
    KRITVA_CHECK(rig.session().reopen().has_value());
    for (const auto& d : rig.node.devices()) for (hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->lifecycle_state() == LifecycleState::FAULT);
    rig.session().close();
    for (const auto& d : rig.node.devices()) for (hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->shutdown().has_value() && e->lifecycle_state() == LifecycleState::STOPPED);
    KRITVA_CHECK(rig.node.live_endpoints() == 0);
    for (const auto& d : rig.node.devices()) {
        for (hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->initialize().has_value() && e->start().has_value());
    }
    KRITVA_CHECK(rig.session().state() == SessionState::CONNECTED && rig.link().link_state() == LinkState::CONNECTED);
    KRITVA_CHECK(cmd.write(hw::MotorCommand{0.25}).has_value() && accel.read(s).has_value());
}

// ---- the existing DeviceManager ----------------------------------------------------------------------------------------------

static void test_remote_devices_run_inside_the_existing_device_manager() {      // RI-001 in principle (formal in I4-007)
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    hw::DeviceManager manager{kritva::core::runtime::ComponentId{200}};
    for (const auto& d : rig.node.devices()) KRITVA_CHECK(manager.register_device(*d).has_value());
    // The declared remote settings are configuration keys like any other.
    const auto keys = manager.config_keys();
    const auto has = [&](const char* k) { return std::find(keys.begin(), keys.end(), k) != keys.end(); };
    KRITVA_CHECK(has("motor.command.max_rad_s") && has("motor.command.fault_after_writes") && has("imu.acceleration.fail_after_ops") && !has("motor.position.initial"));

    auto cfg = config({{"motor.command.min_rad_s", -3}, {"motor.command.max_rad_s", 3}});
    KRITVA_CHECK(cfg.set(kritva::core::Parameter{"motor.enabled", true, {}}).has_value());
    KRITVA_CHECK(manager.configure(cfg).has_value());
    KRITVA_CHECK(rig.motor().command().limits().min_rad_s == -3.0 && rig.motor().command().limits().max_rad_s == 3.0);   // forwarded to the Edge endpoint

    KRITVA_CHECK(manager.initialize().has_value());
    KRITVA_CHECK(rig.edge().session().value() == 2);                       // one fresh session for the whole live period
    for (const auto& [d, e] : EdgeRig::all_endpoints()) {
        const hw::Endpoint* edge_ep = d == kImu ? (e == kAccel ? static_cast<const hw::Endpoint*>(&rig.imu().acceleration()) : &rig.imu().angular_velocity())
                                                : (e == kCommand ? static_cast<const hw::Endpoint*>(&rig.motor().command()) : &rig.motor().position());
        KRITVA_CHECK(edge_ep->lifecycle_state() == LifecycleState::READY);
    }
    KRITVA_CHECK(manager.start().has_value() && manager.lifecycle_state() == LifecycleState::RUNNING);
    KRITVA_CHECK(manager.health().state() == HealthState::HEALTHY);

    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    auto& pos = rig.remote<RemotePositionEndpoint>("motor", "position");
    hw::PositionSample p;
    KRITVA_CHECK(cmd.write(hw::MotorCommand{2.0}).has_value() && pos.read(p).has_value());
    KRITVA_CHECK(cmd.write(hw::MotorCommand{4.0}).error().code == ErrorCode::INVALID_ARGUMENT);      // the Edge's configured limit (3)

    KRITVA_CHECK(manager.stop().has_value() && manager.shutdown().has_value());
    KRITVA_CHECK(rig.motor().command().lifecycle_state() == LifecycleState::STOPPED && rig.motor().command().effective_velocity() == 0.0);
    KRITVA_CHECK(rig.node.live_endpoints() == 0);
    // A second live period is a new session.
    KRITVA_CHECK(manager.initialize().has_value() && manager.start().has_value() && rig.edge().session().value() == 3);
    KRITVA_CHECK(cmd.write(hw::MotorCommand{1.0}).has_value());
    KRITVA_CHECK(manager.stop().has_value() && manager.shutdown().has_value());
}

static void test_the_pending_cleanup_is_done_once() {
    NexusRig rig;
    rig.connect_and_run();
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    rig.link().disconnect();
    for (const auto& d : rig.node.devices()) {                                         // every endpoint is shut down locally: the live period ends
        for (hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(!e->stop().has_value() && e->shutdown().has_value());
    }
    KRITVA_CHECK(rig.node.live_endpoints() == 0);                                      // a cleanup is now pending for each of them
    const auto sent = rig.session().stats().requests_sent;
    KRITVA_CHECK(cmd.initialize().has_value());
    // HELLO, discovery, then the cleanup (STOP and SHUTDOWN) and the INITIALIZE itself.
    KRITVA_CHECK(rig.session().stats().requests_sent - sent == 5);
    KRITVA_CHECK(cmd.start().has_value() && cmd.stop().has_value() && cmd.shutdown().has_value());   // connected: the Edge is told
    const auto sent2 = rig.session().stats().requests_sent;
    KRITVA_CHECK(cmd.initialize().has_value());
    KRITVA_CHECK(rig.session().stats().requests_sent - sent2 == 3);                  // HELLO, discovery, INITIALIZE: no second cleanup
}

static void test_position_samples_carry_the_edges_value_and_time() {
    NexusRig rig;
    rig.connect_and_run();
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    auto& pos = rig.remote<RemotePositionEndpoint>("motor", "position");
    KRITVA_CHECK(cmd.write(hw::MotorCommand{0.75}).has_value());
    hw::PositionSample a, b;
    KRITVA_CHECK(pos.read(a).has_value() && pos.read(b).has_value());
    KRITVA_CHECK(a.value != 0.0 && b.value != a.value && b.value == rig.motor().model().position_rad);   // moving, and the Edge's own value
    KRITVA_CHECK(a.timestamp.nanoseconds() > 0 && b.timestamp.nanoseconds() > a.timestamp.nanoseconds());
    KRITVA_CHECK(a.timestamp.domain() == kritva::core::ClockDomain::MONOTONIC);
}

static void test_an_endpoint_cannot_join_a_live_period_without_a_session() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    auto& pos = rig.remote<RemotePositionEndpoint>("motor", "position");
    KRITVA_CHECK(cmd.initialize().has_value() && rig.node.live_endpoints() == 1);
    rig.link().disconnect();
    const auto r = pos.initialize();                                          // others are live but the session is gone: no new HELLO is sent for it
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::RESOURCE_UNAVAILABLE && pos.lifecycle_state() == LifecycleState::FAULT);
    KRITVA_CHECK(rig.node.live_endpoints() == 1 && rig.session().state() == SessionState::DISCONNECTED);
}

static void test_a_floating_point_setting_is_refused_locally() {
    NexusRig rig;
    KRITVA_CHECK(rig.node.connect().has_value());
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    kritva::core::Configuration c;
    KRITVA_CHECK(c.set(kritva::core::Parameter{"max_rad_s", 2.5, {}}).has_value());
    const auto sent = rig.session().stats().requests_sent;
    const auto r = cmd.configure(c);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT && rig.session().stats().requests_sent == sent);
    // An undeclared name is never sent.
    kritva::core::Configuration d;
    KRITVA_CHECK(d.set(kritva::core::Parameter{"undeclared", std::int64_t{1}, {}}).has_value());
    KRITVA_CHECK(cmd.configure(d).has_value());                                                // nothing to send: an empty configure
    // The Edge's own rejection comes back unchanged.
    const auto direct = rig.motor().command().configure(config({{"min_rad_s", 4}, {"max_rad_s", 2}}));
    KRITVA_CHECK(!direct.has_value());
    KRITVA_CHECK(cmd.configure(config({{"min_rad_s", 4}, {"max_rad_s", 2}})).error().message == direct.error().message);
}

int main() {
    test_connect_builds_ordinary_typed_devices();
    test_lifecycle_maps_to_requests_and_a_fresh_session_per_live_period();
    test_the_i3_state_machine_is_the_endpoints_own();
    test_typed_reads();
    test_a_failing_remote_read_returns_the_edge_error_unchanged();
    test_writes_are_validated_by_the_edge_not_the_nexus();
    test_a_faulted_edge_actuator_faults_the_proxy_through_a_fault_event();
    test_a_timed_out_write_has_an_unknown_outcome_and_retransmission_is_explicit();
    test_initialize_compares_the_topology_as_a_set();
    test_link_loss_faults_the_live_proxies_and_recovery_is_explicit();
    test_remote_devices_run_inside_the_existing_device_manager();
    test_the_pending_cleanup_is_done_once();
    test_position_samples_carry_the_edges_value_and_time();
    test_an_endpoint_cannot_join_a_live_period_without_a_session();
    test_a_floating_point_setting_is_refused_locally();
    std::printf("remote_endpoint_test: PASS\n");
    return 0;
}
