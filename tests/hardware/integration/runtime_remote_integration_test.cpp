//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : runtime_remote_integration_test.cpp
// Description : Integration of the whole production path: RuntimeHost, DeviceManager, remote devices, LoopbackLink, simulated
//               transport, EdgeHost and mock devices; configuration, lifecycle, observation, link loss, driving and recovery.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: RI-001; RI-002; RI-003; DR-001; DR-003; FRL-001; FRL-003 (HUMAN SAFETY REVIEW OPEN)
// API         : RUNTIME-REMOTE-IT
//
// Author      : KritvaOS
// Created     : 06-10-2026
//==============================================================================

#include "../support/stack_rig.hpp"

using namespace kritva;
using namespace kritva::hardware::remote;
using namespace kritva::hardware::remote::test;
using kritva::core::ErrorCode;
using kritva::core::EventType;
using kritva::core::HealthState;
using kritva::core::LifecycleState;
namespace hw = kritva::hardware;

static constexpr std::uint64_t kMs = 1'000'000;

static std::size_t errors_from(const StackRig& rig, core::runtime::ComponentId source) {
    std::size_t n = 0;
    for (const auto& e : rig.events.snapshot()) n += (e.type == EventType::ERROR && e.source_id == source) ? 1 : 0;
    return n;
}

static const RemoteEndpointDiagnostics* find(const RemoteNodeDiagnostics& d, std::uint64_t device, std::uint64_t endpoint) {
    for (const auto& e : d.endpoints) if (e.address.device.value() == device && e.address.endpoint.value() == endpoint) return &e;
    return nullptr;
}

// ---- configuration ------------------------------------------------------------------------------------------------------------

static void test_the_link_timing_is_validated_before_anything_is_sent() {                    // TR-003
    {   // an invalid timing is a CONFIGURATION_ERROR and nothing reaches the link
        EdgeRig edge;
        const auto cfg = runtime::parse_configuration("link.heartbeat_period_ms=200\nlink.heartbeat_timeout_ms=300\n");
        KRITVA_CHECK(cfg.has_value());
        const auto timing = link_timing_from(cfg.value());
        KRITVA_CHECK(!timing.has_value() && timing.error().code == ErrorCode::CONFIGURATION_ERROR);   // the timeout must be at least two periods
        KRITVA_CHECK(edge.link.stats(LinkDirection::NEXUS_TO_EDGE).sent == 0 && edge.link.stats(LinkDirection::EDGE_TO_NEXUS).sent == 0);
    }
    {   // a valid one is proposed in HELLO and accepted by the Edge
        StackRig rig("runtime.name=stack\nlink.heartbeat_period_ms=50\nlink.heartbeat_timeout_ms=200\n");
        KRITVA_CHECK(rig.session().timing().heartbeat_period_ms == 50 && rig.session().timing().heartbeat_timeout_ms == 200);
        KRITVA_CHECK(rig.edge().timing().heartbeat_period_ms == 50 && rig.edge().timing().heartbeat_timeout_ms == 200);
        rig.up();
        // The configured timing governs the supervision on both sides.
        const auto sent = rig.edge().stats().heartbeats_sent;
        KRITVA_CHECK(rig.loop.advance(500 * kMs).has_value() && rig.edge().stats().heartbeats_sent - sent == 10);       // one per 50 ms
        rig.link().disconnect();
        KRITVA_CHECK(rig.loop.advance(200 * kMs).has_value());
        KRITVA_CHECK(!rig.edge().session().valid() && rig.session().state() == SessionState::DISCONNECTED);          // 200 ms, not 300
    }
}

static void test_the_i2_loader_rejects_typos_after_discovery() {
    StackRig rig;
    auto keys = rig.manager.config_keys();
    for (const auto& k : link_config_keys()) keys.push_back(k);
    keys.push_back("runtime.name");
    const auto has = [&](const char* k) { for (const auto& x : keys) if (x == k) return true; return false; };
    KRITVA_CHECK(has("motor.command.max_rad_s") && has("motor.enabled") && has("imu.acceleration.start") && has("link.heartbeat_period_ms") && has("link.pump_quantum_ms"));
    KRITVA_CHECK(!runtime::parse_configuration("runtime.name=x\nmotor.command.max_rad=2\n", keys).has_value());
    KRITVA_CHECK(!runtime::parse_configuration("runtime.name=x\nlink.heartbeat_period=100\n", keys).has_value());
    KRITVA_CHECK(runtime::parse_configuration("runtime.name=x\nmotor.command.max_rad_s=2\nlink.heartbeat_period_ms=100\n", keys).has_value());
}

// ---- the lifecycle through the existing runtime --------------------------------------------------------------------------------

static void test_the_runtime_host_runs_the_remote_devices_through_the_device_manager() {      // RI-001, RI-003
    StackRig rig("runtime.name=stack\nmotor.command.max_rad_s=3\nmotor.command.min_rad_s=-3\n");
    // After connect (HELLO, discovery) and the host's configure (one CONFIGURE per endpoint): 6 requests, nothing else.
    KRITVA_CHECK(rig.session().stats().requests_sent == 6);
    KRITVA_CHECK(rig.motor().command().limits().max_rad_s == 3.0 && rig.motor().command().limits().min_rad_s == -3.0);   // the settings reached the Edge endpoint
    const auto after_configure = rig.session().stats().requests_sent;
    KRITVA_CHECK(rig.host.initialize().has_value());
    KRITVA_CHECK(rig.session().stats().requests_sent - after_configure == 6);             // a fresh session (HELLO, discovery) and one INITIALIZE per endpoint
    KRITVA_CHECK(rig.edge().session().value() == 2 && rig.manager.lifecycle_state() == LifecycleState::READY);
    KRITVA_CHECK(rig.host.start().has_value());
    KRITVA_CHECK(rig.session().stats().requests_sent - after_configure == 10);            // and one START per endpoint
    KRITVA_CHECK(rig.manager.lifecycle_state() == LifecycleState::RUNNING);
    // The application uses the unchanged I3 endpoint API.
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    auto& accel = rig.remote<RemoteAccelerationEndpoint>("imu", "acceleration");
    hw::AccelerationSample a;
    KRITVA_CHECK(accel.read(a).has_value() && a.sequence == 1 && cmd.write(hw::MotorCommand{2.0}).has_value() && rig.motor().model().velocity_rad_s == 2.0);
    KRITVA_CHECK(cmd.write(hw::MotorCommand{4.0}).error().code == ErrorCode::INVALID_ARGUMENT);   // the Edge's configured limit
    // The existing I2 observation and I3 diagnostics show the remote devices through the existing paths.
    const auto obs = rig.host.observe();
    KRITVA_CHECK(obs.has_value() && obs.value().state == LifecycleState::RUNNING && obs.value().components.size() == 1);
    KRITVA_CHECK(obs.value().components[0].observation.lifecycle == LifecycleState::RUNNING && obs.value().components[0].observation.health.state() == HealthState::HEALTHY);
    const auto managed = rig.manager.diagnostics();
    KRITVA_CHECK(managed.size() == 2 && managed[1].name == "motor" && managed[1].endpoints[0].lifecycle == LifecycleState::RUNNING && managed[1].endpoints[0].operations_ok == 1);
    // The reverse order for stop and shutdown, and a second live period is a new session.
    KRITVA_CHECK(rig.host.stop().has_value() && rig.motor().command().lifecycle_state() == LifecycleState::STOPPED && rig.motor().command().effective_velocity() == 0.0);
    KRITVA_CHECK(rig.host.shutdown().has_value() && rig.node->live_endpoints() == 0);
    KRITVA_CHECK(rig.host.initialize().has_value() && rig.host.start().has_value() && rig.edge().session().value() == 3);
    KRITVA_CHECK(cmd.write(hw::MotorCommand{1.0}).has_value());
    KRITVA_CHECK(rig.host.stop().has_value() && rig.host.shutdown().has_value());
}

// ---- failure through the runtime -------------------------------------------------------------------------------------------------

static void test_link_loss_reaches_the_runtime_event_log_once_per_endpoint() {               // FRL-001
    StackRig rig;
    rig.up();
    const auto manager_id = rig.manager.info().id();
    KRITVA_CHECK(errors_from(rig, manager_id) == 0);
    KRITVA_CHECK(rig.remote<RemoteMotorCommandEndpoint>("motor", "command").write(hw::MotorCommand{0.5}).has_value());
    rig.link().disconnect();                                                                   // the failure is injected into the transport
    KRITVA_CHECK(rig.loop.advance(400 * kMs).has_value());                                     // and the time is spent, explicitly
    KRITVA_CHECK(errors_from(rig, manager_id) == 4 && rig.error_events() == 4);                // N endpoint faults, N ERROR events from the manager
    KRITVA_CHECK(rig.node->link_loss_records().size() == 1 && rig.node->link_loss_records()[0].endpoints_faulted == 4);   // plus one link record
    const auto nexus = diagnose(*rig.node);
    for (const auto& e : nexus.endpoints) KRITVA_CHECK(e.fault_origin == FaultOrigin::LINK_LOST && e.endpoint.health_detail == "link lost");
    // The Edge acted by itself: the actuator is at safe zero, sensors untouched.
    const auto edge = diagnose(rig.edge());
    KRITVA_CHECK(!edge.session.valid() && edge.stats.sessions_timed_out == 1 && edge.devices[1].endpoints[0].lifecycle == LifecycleState::STOPPED);
    KRITVA_CHECK(rig.motor().command().effective_velocity() == 0.0 && edge.devices[0].endpoints[0].lifecycle == LifecycleState::RUNNING);
    // The manager keeps its lifecycle and reports it through the existing health path.
    const auto obs = rig.host.observe();
    KRITVA_CHECK(obs.has_value() && obs.value().components[0].observation.lifecycle == LifecycleState::RUNNING &&
                 obs.value().components[0].observation.health.state() == HealthState::UNHEALTHY);
    // More driving, observation and diagnostics change nothing.
    for (int i = 0; i < 20; ++i) {
        KRITVA_CHECK(rig.loop.advance(100 * kMs).has_value());
        (void)rig.host.observe();
        (void)rig.manager.diagnostics();
        (void)describe(diagnose(*rig.node));
    }
    KRITVA_CHECK(errors_from(rig, manager_id) == 4 && rig.node->link_loss_records().size() == 1);
}

static void test_a_remote_fault_reaches_the_runtime_event_log_once() {
    StackRig rig;
    rig.up();
    KRITVA_CHECK(rig.motor().command().inject_fault("overcurrent").has_value());
    KRITVA_CHECK(rig.loop.advance(5 * kMs).has_value());
    KRITVA_CHECK(errors_from(rig, rig.manager.info().id()) == 1);
    const auto d = diagnose(*rig.node);
    KRITVA_CHECK(find(d, kMotor, kCommand)->fault_origin == FaultOrigin::REMOTE_FAULT && d.link_state == SessionState::CONNECTED);
    KRITVA_CHECK(rig.loop.advance(500 * kMs).has_value() && errors_from(rig, rig.manager.info().id()) == 1);   // told once, and the link is fine
}

// ---- driving is explicit -----------------------------------------------------------------------------------------------------------

static void test_nothing_is_supervised_unless_somebody_drives_it() {                       // approved constraint 3
    StackRig rig;
    rig.up();
    KRITVA_CHECK(rig.remote<RemoteMotorCommandEndpoint>("motor", "command").write(hw::MotorCommand{0.5}).has_value());
    const auto before = describe(diagnose(*rig.node)) + describe(diagnose(rig.edge()));
    KRITVA_CHECK(rig.link().advance(10'000 * kMs).has_value());                                // ten virtual seconds, nobody drives
    KRITVA_CHECK(rig.session().state() == SessionState::CONNECTED && rig.edge().session().valid() && rig.motor().command().lifecycle_state() == LifecycleState::RUNNING);
    KRITVA_CHECK(rig.error_events() == 0 && rig.node->link_loss_records().empty());
    (void)before;
    rig.loop.pump();                                                                           // one explicit round: the Edge polls, the Nexus is serviced
    KRITVA_CHECK(!rig.edge().session().valid() && rig.motor().command().lifecycle_state() == LifecycleState::STOPPED);
    KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED && rig.error_events() == 4);
}

static void test_the_edge_may_be_safe_while_the_nexus_does_not_know_yet() {                  // approved constraint 8
    StackRig rig;
    rig.up();
    KRITVA_CHECK(rig.remote<RemoteMotorCommandEndpoint>("motor", "command").write(hw::MotorCommand{0.5}).has_value());
    KRITVA_CHECK(rig.link().advance(300 * kMs).has_value());
    rig.edge().poll();                                                                        // only the Edge is driven
    const auto edge = diagnose(rig.edge());
    const auto nexus = diagnose(*rig.node);
    KRITVA_CHECK(edge.devices[1].endpoints[0].lifecycle == LifecycleState::STOPPED && rig.motor().command().effective_velocity() == 0.0);
    KRITVA_CHECK(find(nexus, kMotor, kCommand)->endpoint.lifecycle == LifecycleState::RUNNING && nexus.link_state == SessionState::CONNECTED);
    KRITVA_CHECK(rig.host.observe().value().components[0].observation.health.state() == HealthState::HEALTHY);   // and the runtime says so, honestly
    KRITVA_CHECK(rig.error_events() == 0);
    KRITVA_CHECK(rig.loop.advance(1 * kMs).has_value());                                       // the Nexus is driven: now it knows
    KRITVA_CHECK(rig.error_events() == 4 && diagnose(*rig.node).link_state == SessionState::DISCONNECTED);
}

static void test_close_is_nexus_local_and_the_edge_stops_the_actuator_by_itself() {
    StackRig rig;
    rig.up();
    KRITVA_CHECK(rig.remote<RemoteMotorCommandEndpoint>("motor", "command").write(hw::MotorCommand{0.5}).has_value());
    rig.session().close();
    KRITVA_CHECK(rig.error_events() == 4 && diagnose(*rig.node).endpoints[0].fault_origin == FaultOrigin::SESSION_CLOSED);
    KRITVA_CHECK(diagnose(rig.edge()).session.valid() && rig.motor().command().effective_velocity() == 0.5);   // the Edge is unaware
    KRITVA_CHECK(rig.loop.advance(299 * kMs).has_value() && rig.motor().command().effective_velocity() == 0.5);
    KRITVA_CHECK(rig.loop.advance(1 * kMs).has_value());                                       // its own timeout
    KRITVA_CHECK(rig.motor().command().effective_velocity() == 0.0 && rig.motor().command().lifecycle_state() == LifecycleState::STOPPED);
}

// ---- recovery is explicit ----------------------------------------------------------------------------------------------------------------

static void test_nothing_recovers_by_itself_and_the_runtime_way_back_works() {              // FRL-003, approved constraint 9
    StackRig rig;
    rig.up();
    rig.link().disconnect();
    KRITVA_CHECK(rig.loop.advance(400 * kMs).has_value());
    const auto sent = rig.session().stats().requests_sent;
    KRITVA_CHECK(rig.loop.advance(3000 * kMs).has_value());
    for (int i = 0; i < 10; ++i) { (void)rig.host.observe(); (void)rig.manager.diagnostics(); (void)describe(diagnose(*rig.node)); (void)describe(diagnose(rig.edge())); }
    KRITVA_CHECK(rig.session().state() == SessionState::DISCONNECTED && rig.session().stats().requests_sent == sent);   // no reconnect, nothing sent
    KRITVA_CHECK(rig.link().link_state() == LinkState::DISCONNECTED);                                                  // the link stays down
    for (const auto& d : rig.node->devices()) for (const hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->lifecycle_state() == LifecycleState::FAULT);
    // The explicit way back through the runtime: stop, shutdown, initialize (a fresh HELLO and discovery), start.
    KRITVA_CHECK(rig.host.stop().has_value() && rig.host.shutdown().has_value());
    for (const auto& d : rig.node->devices()) for (const hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->lifecycle_state() == LifecycleState::STOPPED);
    KRITVA_CHECK(rig.host.initialize().has_value() && rig.host.start().has_value());
    KRITVA_CHECK(rig.session().state() == SessionState::CONNECTED && rig.edge().session().valid());
    for (const auto& e : diagnose(*rig.node).endpoints) KRITVA_CHECK(e.fault_origin == FaultOrigin::NONE && e.endpoint.lifecycle == LifecycleState::RUNNING);
    KRITVA_CHECK(rig.remote<RemoteMotorCommandEndpoint>("motor", "command").write(hw::MotorCommand{0.25}).has_value() && rig.motor().command().effective_velocity() == 0.25);
    KRITVA_CHECK(rig.host.observe().value().components[0].observation.health.state() == HealthState::HEALTHY);
    KRITVA_CHECK(rig.node->link_loss_records().size() == 1);                                   // the history stays
}

int main() {
    test_the_link_timing_is_validated_before_anything_is_sent();
    test_the_i2_loader_rejects_typos_after_discovery();
    test_the_runtime_host_runs_the_remote_devices_through_the_device_manager();
    test_link_loss_reaches_the_runtime_event_log_once_per_endpoint();
    test_a_remote_fault_reaches_the_runtime_event_log_once();
    test_nothing_is_supervised_unless_somebody_drives_it();
    test_the_edge_may_be_safe_while_the_nexus_does_not_know_yet();
    test_close_is_nexus_local_and_the_edge_stops_the_actuator_by_itself();
    test_nothing_recovers_by_itself_and_the_runtime_way_back_works();
    std::printf("runtime_remote_integration_test: PASS\n");
    return 0;
}
