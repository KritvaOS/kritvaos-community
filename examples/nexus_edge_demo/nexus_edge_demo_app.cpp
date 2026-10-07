//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : nexus_edge_demo_app.cpp
// Description : The KOS-I4 reference application scenario.
//
// Component   : KritvaOS Examples
// Module      : Nexus-Edge Demo
// Layer       : Application
//
// Requirements: VR-001; VR-002; VR-003
// API         : kritva::demo::NexusEdgeDemoApplication
//
// Author      : KritvaOS
// Created     : 07-10-2026
//==============================================================================

#include "nexus_edge_demo_app.hpp"

#include <cstdio>
#include <memory>
#include <string>

#include <kritva/hardware/device_manager.hpp>
#include <kritva/hardware/mock/mock_imu.hpp>
#include <kritva/hardware/mock/mock_motor.hpp>
#include <kritva/hardware/remote/edge_diagnostics.hpp>
#include <kritva/hardware/remote/edge_host.hpp>
#include <kritva/hardware/remote/loopback_link.hpp>
#include <kritva/hardware/remote/remote_diagnostics.hpp>
#include <kritva/hardware/remote/remote_node.hpp>
#include <kritva/hardware/transport/link_config.hpp>
#include <kritva/hardware/transport/simulated_transport.hpp>
#include <kritva/runtime/runtime_host.hpp>

namespace kritva::demo {

namespace {

namespace hw = kritva::hardware;
namespace remote = kritva::hardware::remote;
namespace transport = kritva::hardware::transport;
using core::LifecycleState;
using runtime::to_string;

constexpr core::runtime::ComponentId kManagerId{100};
constexpr std::uint64_t kNsPerMs = 1'000'000;
constexpr std::uint64_t kEdgeNode = 7;
constexpr std::uint64_t kNexusNode = 3;

std::string fixed(double v, int digits) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", digits, v);
    return buffer;
}

// The settings the Nexus may send to the Edge's endpoints (protocol 1.0 does not carry the Edge's setting names).
remote::RemoteSettings demo_settings() {
    return {{{"left_arm_imu", "acceleration"}, {"start", "step"}},
            {{"left_arm_imu", "angular_velocity"}, {"start", "step"}},
            {{"shoulder_motor", "command"}, {"min_rad_s", "max_rad_s"}},
            {{"shoulder_motor", "position"}, {"initial"}}};
}

} // namespace

std::vector<std::string> nexus_edge_demo_config_keys() {
    std::vector<std::string> keys = {"runtime.name", "demo.link_latency_ms", "demo.command_milli_rad_s", "demo.run_ms", "demo.hostile_link"};
    for (const auto& k : transport::link_config_keys()) keys.push_back(k);
    for (const char* device : {"left_arm_imu", "shoulder_motor"}) keys.push_back(std::string(device) + ".enabled");
    for (const auto& [endpoint, settings] : demo_settings()) {
        for (const auto& s : settings) keys.push_back(endpoint.first + "." + endpoint.second + "." + s);
    }
    return keys;
}

NexusEdgeDemoOutcome NexusEdgeDemoApplication::run(const core::Configuration& configuration) {
    // ---- demo-level settings, validated before anything is constructed -------------------------------------------------------
    const auto latency_ms = runtime::get_int(configuration, "demo.link_latency_ms", 1, 0, 50);
    const auto command_milli = runtime::get_int(configuration, "demo.command_milli_rad_s", 500, 1, 1'000'000);
    const auto run_ms = runtime::get_int(configuration, "demo.run_ms", 500, 10, 5000);
    const auto hostile = runtime::get_bool(configuration, "demo.hostile_link", true);
    const auto timing = transport::link_timing_from(configuration);
    const auto config_error = [&](const std::string& message) {
        out_ << "[nexus_edge_demo] CONFIG ERROR: " << message << "\n";
        return NexusEdgeDemoOutcome::ERROR;
    };
    if (!latency_ms) return config_error(latency_ms.error().message);
    if (!command_milli) return config_error(command_milli.error().message);
    if (!run_ms) return config_error(run_ms.error().message);
    if (!hostile) return config_error(hostile.error().message);
    if (!timing) return config_error(timing.error().message);

    // ---- the Edge: mock hardware behind an EdgeHost, on the far end of a simulated transport ------------------------------------
    // (Devices are declared before everything that refers to them: they must outlive it.)
    hw::mock::MockImuDevice imu(hw::DeviceInfo::create(hw::DeviceId{1}, "left_arm_imu").value());
    hw::mock::MockMotorDevice motor(hw::DeviceInfo::create(hw::DeviceId{2}, "shoulder_motor").value());
    hw::DeviceRegistry edge_registry;
    transport::SimulatedTransport link;
    (void)edge_registry.register_device(imu);
    (void)edge_registry.register_device(motor);
    auto edge_host = remote::EdgeHost::create(remote::NodeId{kEdgeNode}, edge_registry, link.edge());
    if (!edge_host) return config_error(edge_host.error().message);
    remote::EdgeHost& edge = *edge_host.value();

    // ---- the Nexus: the node, the DeviceManager and the RuntimeHost; the LoopbackLink is the one explicit driver --------------------
    remote::LoopbackLink loop(link, edge, static_cast<std::uint64_t>(timing.value().pump_quantum_ms) * kNsPerMs);
    remote::RemoteNodeConfig node_config;
    node_config.nexus_node = remote::NodeId{kNexusNode};
    node_config.edge_node = remote::NodeId{kEdgeNode};
    node_config.timing = timing.value();
    node_config.peer_tick = loop.peer_tick();
    remote::RemoteNode node(link.nexus(), std::move(node_config), demo_settings());
    loop.attach(node);

    hw::DeviceManager manager{kManagerId};
    runtime::EventLog events;
    runtime::RuntimeHost host;
    host.set_event_sink(&events);
    manager.set_event_sink(&events);

    const auto say = [&](const std::string& line) { out_ << "[nexus_edge_demo] t=" << link.now_ns() / kNsPerMs << "ms " << line << "\n"; };
    const auto fail = [&](const std::string& what, const std::string& message) {
        say(what + ": " + message);
        (void)host.controlled_shutdown();
        return NexusEdgeDemoOutcome::ERROR;
    };
    // An expectation: it is printed, and the first one that does not hold ends the scenario.
    bool all_ok = true;
    const auto expect = [&](bool condition, const std::string& what) {
        say(std::string(condition ? "ok: " : "EXPECTATION FAILED: ") + what);
        if (!condition) all_ok = false;
        return condition;
    };
    const auto bail = [&] { return fail("SCENARIO ABORTED", "an expectation did not hold"); };

    // The link: latency in both directions; faults are configured per direction (and reset by configuring again).
    const auto set_link = [&](const transport::DirectionConfig& to_edge, const transport::DirectionConfig& to_nexus) {
        return link.configure(transport::LinkDirection::NEXUS_TO_EDGE, to_edge).has_value() &&
               link.configure(transport::LinkDirection::EDGE_TO_NEXUS, to_nexus).has_value();
    };
    transport::DirectionConfig calm;
    calm.latency_ns = static_cast<std::uint64_t>(latency_ms.value()) * kNsPerMs;
    if (!set_link(calm, calm)) return config_error("invalid link configuration");

    say("composition: Edge = mock IMU + mock motor behind an EdgeHost (node " + std::to_string(kEdgeNode) + "); Nexus = RuntimeHost -> DeviceManager -> RemoteDevice -> RemoteEndpoint -> RemoteNode (node " +
        std::to_string(kNexusNode) + ") -> LoopbackLink -> SimulatedTransport");
    say("link timing: heartbeat " + std::to_string(timing.value().heartbeat_period_ms) + " ms, timeout " + std::to_string(timing.value().heartbeat_timeout_ms) +
        " ms, request timeout " + std::to_string(timing.value().request_timeout_ms) + " ms, latency " + std::to_string(latency_ms.value()) + " ms each way");

    // ---- 1. connect, discover, configure -------------------------------------------------------------------------------------------
    if (auto r = node.connect(); !r) return fail("CONNECT FAILED", r.error().message);
    say("HELLO accepted, discovery complete: " + std::to_string(node.devices().size()) + " remote devices");
    for (const auto& d : node.devices()) {
        for (const hw::Endpoint* e : d->endpoints()) {
            std::string caps;
            for (const auto& c : e->capabilities().all()) caps += (caps.empty() ? "" : ",") + c.name;
            say("discovered " + d->info().name() + "." + e->info().name() + " (" + (e->info().direction() == hw::EndpointDirection::SENSOR ? "sensor" : "actuator") +
                ", id=" + std::to_string(e->info().id().value()) + ") capabilities=" + caps);
        }
    }
    for (const auto& d : node.devices()) {
        if (auto r = manager.register_device(*d); !r) return fail("REGISTER ERROR", r.error().message);
    }
    if (auto r = host.add_component(manager); !r) return fail("COMPOSE ERROR", r.error().message);
    if (auto r = host.configure(configuration); !r) return fail("CONFIG ERROR", r.error().message);
    say("config: " + runtime::describe_configuration(host.settings(), configuration.size()));
    say("registered devices: " + std::to_string(manager.registry().size()));

    // The application finds its endpoints in the DeviceManager and uses only the unchanged I3 typed interfaces.
    struct Found { hw::AccelerationEndpoint* accel; hw::AngularVelocityEndpoint* gyro; hw::MotorCommandEndpoint* command; hw::PositionEndpoint* position; } found{};
    const auto find = [&](const char* device, const char* endpoint) -> hw::Endpoint* {
        const auto e = manager.registry().find_endpoint(device, endpoint);
        return e ? e.value() : nullptr;
    };
    found.accel = dynamic_cast<hw::AccelerationEndpoint*>(find("left_arm_imu", "acceleration"));
    found.gyro = dynamic_cast<hw::AngularVelocityEndpoint*>(find("left_arm_imu", "angular_velocity"));
    found.command = dynamic_cast<hw::MotorCommandEndpoint*>(find("shoulder_motor", "command"));
    found.position = dynamic_cast<hw::PositionEndpoint*>(find("shoulder_motor", "position"));
    if (found.accel == nullptr || found.gyro == nullptr || found.command == nullptr || found.position == nullptr) {
        return fail("DISCOVERY ERROR", "an endpoint does not have the expected type");
    }

    // ---- 2. initialize and start through the RuntimeHost ----------------------------------------------------------------------------
    say(std::string("state=") + std::string(to_string(host.state())));
    if (auto r = host.initialize(); !r) return fail("INITIALIZE FAILED", r.error().message);
    say(std::string("state=") + std::string(to_string(host.state())) + " (a fresh session; the Edge endpoints are READY)");
    if (auto r = host.start(); !r) return fail("START FAILED", r.error().message);
    say(std::string("state=") + std::string(to_string(host.state())));
    const auto session_one = remote::diagnose(node).session.value();
    {
        const auto edge_view = remote::diagnose(edge);
        bool running = true;
        for (const auto& d : edge_view.devices) for (const auto& e : d.endpoints) running = running && e.lifecycle == LifecycleState::RUNNING;
        if (!expect(running && edge_view.session.value() == session_one && remote::diagnose(node).link_state == transport::SessionState::CONNECTED,
                    "the Edge endpoints are RUNNING in session " + std::to_string(session_one) + " and the Nexus link is CONNECTED")) return bail();
    }

    // ---- 3. use the endpoints; the Edge validates and owns the actuator -------------------------------------------------------------------
    hw::AccelerationSample accel;
    hw::AngularVelocitySample gyro;
    hw::PositionSample position;
    if (auto r = found.accel->read(accel); !r) return fail("READ FAILED", r.error().message);
    if (auto r = found.gyro->read(gyro); !r) return fail("READ FAILED", r.error().message);
    if (auto r = found.position->read(position); !r) return fail("READ FAILED", r.error().message);
    say("sensors: accel(" + fixed(accel.value.x, 2) + "," + fixed(accel.value.y, 2) + "," + fixed(accel.value.z, 2) + ") #" + std::to_string(accel.sequence) +
        " gyro(" + fixed(gyro.value.x, 2) + ") #" + std::to_string(gyro.sequence) + " position " + fixed(position.value, 3) + " rad #" + std::to_string(position.sequence));
    if (!expect(accel.sequence == 1 && gyro.sequence == 1 && position.sequence == 1, "each sensor delivered its first sample through the remote endpoint")) return bail();
    const double command = static_cast<double>(command_milli.value()) / 1000.0;
    if (auto r = found.command->write(hw::MotorCommand{command}); !r) return fail("WRITE FAILED", r.error().message);
    say("motor command " + fixed(command, 3) + " rad/s accepted by the Edge");
    if (!expect(motor.command().effective_velocity() == command, "the Edge applied the command: the motor runs at " + fixed(motor.command().effective_velocity(), 3) + " rad/s")) return bail();
    const auto too_fast = found.command->write(hw::MotorCommand{1000.0});
    if (!expect(!too_fast && too_fast.error().code == core::ErrorCode::INVALID_ARGUMENT && motor.command().effective_velocity() == command,
                "an out-of-limit command is refused by the Edge's own validation (the Nexus knows no limit) and changes nothing")) return bail();
    say("runtime observation:\n" + runtime::describe(host.observe().value()));
    say("device diagnostics (the existing I3 path):\n" + hw::describe(manager.diagnostics()));

    // ---- 4. heartbeats ----------------------------------------------------------------------------------------------------------------
    const auto nexus_before = remote::diagnose(node);
    const auto edge_before = remote::diagnose(edge);
    if (auto r = loop.advance(static_cast<std::uint64_t>(run_ms.value()) * kNsPerMs); !r) return fail("ADVANCE FAILED", r.error().message);
    const auto nexus_after = remote::diagnose(node);
    const auto edge_after = remote::diagnose(edge);
    const std::uint64_t periods = static_cast<std::uint64_t>(run_ms.value()) / timing.value().heartbeat_period_ms;
    say("heartbeats over " + std::to_string(run_ms.value()) + " ms: Nexus sent " + std::to_string(nexus_after.link_stats.heartbeats_sent - nexus_before.link_stats.heartbeats_sent) +
        ", Edge sent " + std::to_string(edge_after.stats.heartbeats_sent - edge_before.stats.heartbeats_sent));
    if (!expect(nexus_after.link_stats.heartbeats_sent - nexus_before.link_stats.heartbeats_sent + 1 >= periods &&
                    edge_after.stats.heartbeats_sent - edge_before.stats.heartbeats_sent + 1 >= periods && nexus_after.link_stats.degraded_entries == 0 &&
                    nexus_after.link_state == transport::SessionState::CONNECTED && edge_after.link_state == transport::SessionState::CONNECTED,
                "both sides kept sending heartbeats and neither link degraded")) return bail();
    hw::PositionSample moved;
    if (auto r = found.position->read(moved); !r) return fail("READ FAILED", r.error().message);
    if (!expect(moved.value > position.value, "the position advanced under the commanded velocity (" + fixed(position.value, 3) + " -> " + fixed(moved.value, 3) + " rad)")) return bail();

    // ---- 5. hostile traffic on the link: duplicates and replays ---------------------------------------------------------------------------
    if (hostile.value()) {
        say("hostile link: duplicated and replayed frames");
        const auto edge_base = remote::diagnose(edge).stats;
        const auto nexus_base = remote::diagnose(node).link_stats;
        // The Edge's own count of successful writes on the actuator endpoint (the I3 counter): the proof that a command reached the
        // hardware interface exactly as often as it should.
        const auto actuator_writes = [&] { return remote::diagnose(edge).devices[1].endpoints[0].operations_ok; };
        const auto writes_base = actuator_writes();
        transport::DirectionConfig duplicating = calm;
        duplicating.faults[0].duplicate = true;                                              // the next frame the Nexus sends arrives twice
        if (!set_link(duplicating, calm)) return config_error("invalid link configuration");
        if (auto r = found.command->write(hw::MotorCommand{command * 1.2}); !r) return fail("WRITE FAILED", r.error().message);
        if (auto r = loop.advance(10 * kNsPerMs); !r) return fail("ADVANCE FAILED", r.error().message);
        const auto edge_dup = remote::diagnose(edge).stats;
        if (!expect(edge_dup.writes_applied - edge_base.writes_applied == 1 && edge_dup.writes_resent - edge_base.writes_resent == 1,
                    "a duplicated write was applied once; the Edge answered the duplicate from its ledger (applied +1, resent +1)")) return bail();
        if (!expect(remote::diagnose(node).link_stats.unknown_correlation - nexus_base.unknown_correlation >= 1,
                    "the Nexus rejected the duplicate answer (an answer that matches no outstanding request)")) return bail();
        if (!expect(actuator_writes() - writes_base == 1, "the actuator endpoint itself counted exactly one write for the duplicated command")) return bail();

        // An answer that is duplicated and arrives after its request is complete is rejected as well.
        transport::DirectionConfig echo = calm;
        echo.faults[0].duplicate = true;
        echo.faults[0].duplicate_extra_delay_ns = 20 * kNsPerMs;
        if (!set_link(calm, echo)) return config_error("invalid link configuration");
        const auto unknown_before = remote::diagnose(node).link_stats.unknown_correlation;
        hw::AccelerationSample echoed;
        if (auto r = found.accel->read(echoed); !r) return fail("READ FAILED", r.error().message);
        if (auto r = loop.advance(30 * kNsPerMs); !r) return fail("ADVANCE FAILED", r.error().message);
        if (!expect(remote::diagnose(node).link_stats.unknown_correlation - unknown_before == 1 && echoed.sequence == accel.sequence + 1,
                    "a duplicate answer that arrived late was rejected, and the read it repeated was taken once")) return bail();

        transport::DirectionConfig replaying = calm;
        replaying.faults[0].duplicate = true;
        replaying.faults[0].duplicate_extra_delay_ns = 20 * kNsPerMs;                        // a copy of the next frame arrives 20 ms later
        if (!set_link(replaying, calm)) return config_error("invalid link configuration");
        const double older = command * 0.4;
        const double newer = command * 0.8;
        if (auto r = found.command->write(hw::MotorCommand{older}); !r) return fail("WRITE FAILED", r.error().message);
        if (auto r = found.command->write(hw::MotorCommand{newer}); !r) return fail("WRITE FAILED", r.error().message);
        if (auto r = loop.advance(40 * kNsPerMs); !r) return fail("ADVANCE FAILED", r.error().message);
        const auto edge_replay = remote::diagnose(edge).stats;
        say("replay: the copy of the older write (" + fixed(older, 3) + ") arrived after the newer one (" + fixed(newer, 3) + ")");
        if (!expect(edge_replay.stale_frames - edge_dup.stale_frames == 1 && edge_replay.writes_applied - edge_dup.writes_applied == 2,
                    "the replayed older write was dropped as stale and never applied (applied +2, stale +1)")) return bail();
        if (!expect(motor.command().effective_velocity() == newer, "the motor still runs at the newer command " + fixed(newer, 3) + " rad/s")) return bail();
        if (!expect(actuator_writes() - writes_base == 3, "the actuator endpoint counted exactly the three writes that were applied (the duplicated one once, the older and the newer one once each)")) return bail();
        if (!set_link(calm, calm)) return config_error("invalid link configuration");
        if (auto r = found.command->write(hw::MotorCommand{command}); !r) return fail("WRITE FAILED", r.error().message);
        if (!expect(motor.command().effective_velocity() == command, "the actuator is controllable again; the link is back to normal")) return bail();
    }

    // ---- 6. the link is lost ------------------------------------------------------------------------------------------------------------
    const auto error_count = [&] {
        std::size_t n = 0;
        for (const auto& e : events.snapshot()) n += e.type == core::EventType::ERROR ? 1 : 0;
        return n;
    };
    const auto errors_before = error_count();
    say("injecting link loss: the simulated transport goes down");
    link.disconnect();
    if (auto r = loop.advance(10 * kNsPerMs); !r) return fail("ADVANCE FAILED", r.error().message);
    {
        const auto nexus = remote::diagnose(node);
        bool all_faulted = !nexus.endpoints.empty();
        for (const auto& e : nexus.endpoints) {
            all_faulted = all_faulted && e.endpoint.lifecycle == LifecycleState::FAULT && e.fault_origin == remote::FaultOrigin::LINK_LOST && e.endpoint.health_detail == "link lost";
        }
        if (!expect(nexus.link_state == transport::SessionState::DISCONNECTED && all_faulted,
                    "10 ms later the Nexus knows: the link is DISCONNECTED and every live remote endpoint is FAULT, reason \"link lost\"")) return bail();
        if (!expect(error_count() - errors_before == nexus.endpoints.size() && nexus.link_loss_records.size() == 1 && nexus.link_loss_records[0].endpoints_faulted == nexus.endpoints.size(),
                    "one ERROR event per endpoint (" + std::to_string(nexus.endpoints.size()) + ") and one link-level record")) return bail();
    }
    if (!expect(motor.command().effective_velocity() == command && remote::diagnose(edge).session.valid(),
                "the Edge has not timed out yet: its actuator still runs (the Edge and the Nexus views differ, and nothing hides it)")) return bail();
    if (auto r = loop.advance(400 * kNsPerMs); !r) return fail("ADVANCE FAILED", r.error().message);
    {
        const auto edge_view = remote::diagnose(edge);
        if (!expect(!edge_view.session.valid() && edge_view.stats.sessions_timed_out == 1 && edge_view.devices[1].endpoints[0].lifecycle == LifecycleState::STOPPED &&
                        motor.command().effective_velocity() == 0.0,
                    "the Edge's heartbeat timeout expired: it stopped its own actuator through the I3 stop(); the motor is at safe zero")) return bail();
        if (!expect(edge_view.devices[0].endpoints[0].lifecycle == LifecycleState::RUNNING && edge_view.devices[1].endpoints[1].lifecycle == LifecycleState::RUNNING,
                    "the Edge's sensors keep running: only actuators are stopped by the loss of the link")) return bail();
    }
    say("Nexus diagnostics after the loss:\n" + remote::describe(remote::diagnose(node)));
    say("Edge diagnostics after the loss:\n" + remote::describe(remote::diagnose(edge)));
    const auto health = host.observe();
    if (!expect(health && health.value().components[0].observation.health.state() == core::HealthState::UNHEALTHY, "the runtime reports the DeviceManager UNHEALTHY through the existing observation path")) return bail();

    // ---- 7. nothing recovers by itself ------------------------------------------------------------------------------------------------------
    const auto idle_before = remote::diagnose(node);
    if (auto r = loop.advance(2000 * kNsPerMs); !r) return fail("ADVANCE FAILED", r.error().message);
    for (int i = 0; i < 5; ++i) { (void)host.observe(); (void)manager.diagnostics(); (void)remote::describe(remote::diagnose(node)); (void)remote::describe(remote::diagnose(edge)); }
    {
        const auto idle_after = remote::diagnose(node);
        bool still_faulted = true;
        for (const auto& e : idle_after.endpoints) still_faulted = still_faulted && e.endpoint.lifecycle == LifecycleState::FAULT;
        if (!expect(idle_after.link_state == transport::SessionState::DISCONNECTED && idle_after.link_stats.requests_sent == idle_before.link_stats.requests_sent && still_faulted &&
                        link.link_state() == transport::LinkState::DISCONNECTED && error_count() - errors_before == idle_after.endpoints.size(),
                    "2 s later, after observation and diagnostics: no reconnect, nothing sent, the endpoints are still FAULT, no new event")) return bail();
    }

    // ---- 8. explicit recovery through the runtime: stop, shutdown, initialize (fresh HELLO and discovery), start ------------------------------------
    say("recovery: stop, shutdown, initialize, start");
    if (auto r = host.stop(); !r) return fail("STOP FAILED", r.error().message);
    if (auto r = host.shutdown(); !r) return fail("SHUTDOWN FAILED", r.error().message);
    if (auto r = host.initialize(); !r) return fail("INITIALIZE FAILED", r.error().message);
    if (auto r = host.start(); !r) return fail("START FAILED", r.error().message);
    {
        const auto nexus = remote::diagnose(node);
        bool running = true;
        for (const auto& e : nexus.endpoints) running = running && e.endpoint.lifecycle == LifecycleState::RUNNING && e.fault_origin == remote::FaultOrigin::NONE;
        if (!expect(nexus.link_state == transport::SessionState::CONNECTED && nexus.session.value() > session_one && running,
                    "a fresh session " + std::to_string(nexus.session.value()) + " (was " + std::to_string(session_one) + "): every endpoint is RUNNING again, the faults are cleared only by the lifecycle")) return bail();
        if (!expect(nexus.link_loss_records.size() == 1, "the history of the loss is kept")) return bail();
    }
    if (auto r = found.accel->read(accel); !r) return fail("READ FAILED", r.error().message);
    if (auto r = found.command->write(hw::MotorCommand{command * 0.6}); !r) return fail("WRITE FAILED", r.error().message);
    if (!expect(motor.command().effective_velocity() == command * 0.6, "the second live period works: the Edge applied " + fixed(command * 0.6, 3) + " rad/s")) return bail();
    {
        const auto before = remote::diagnose(node).link_stats.heartbeats_sent;
        if (auto r = loop.advance(2 * timing.value().heartbeat_period_ms * kNsPerMs); !r) return fail("ADVANCE FAILED", r.error().message);
        if (!expect(remote::diagnose(node).link_stats.heartbeats_sent > before && remote::diagnose(node).link_state == transport::SessionState::CONNECTED, "heartbeats flow again")) return bail();
    }

    // ---- 9. controlled shutdown -----------------------------------------------------------------------------------------------------------------
    if (auto r = host.stop(); !r) return fail("STOP FAILED", r.error().message);
    if (auto r = host.shutdown(); !r) return fail("SHUTDOWN FAILED", r.error().message);
    say(std::string("state=") + std::string(to_string(host.state())));
    {
        const auto edge_view = remote::diagnose(edge);
        bool stopped = true;
        for (const auto& d : edge_view.devices) for (const auto& e : d.endpoints) stopped = stopped && e.lifecycle == LifecycleState::STOPPED;
        if (!expect(host.state() == LifecycleState::STOPPED && stopped && motor.command().effective_velocity() == 0.0, "everything is STOPPED, on the Nexus and on the Edge")) return bail();
    }
    say("controlled shutdown complete (error events=" + std::to_string(error_count() - errors_before) + ")");
    if (!all_ok) return fail("RESULT", "an expectation did not hold");
    say("RESULT: PASS");
    return NexusEdgeDemoOutcome::COMPLETED;
}

} // namespace kritva::demo
