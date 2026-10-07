//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : nexus_edge_demo_app.hpp
// Description : The KOS-I4 reference application: a Nexus and an Edge in one process, from the RuntimeHost to mock hardware.
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

#pragma once

#include <ostream>
#include <string>
#include <vector>

#include <kritva/core/core.hpp>

namespace kritva::demo {

enum class NexusEdgeDemoOutcome {
    COMPLETED,   ///< The scenario ran and every expectation held.
    ERROR,       ///< A configuration, lifecycle or expectation error; the runtime was shut down.
};

/// Process exit code: COMPLETED 0, ERROR 1.
[[nodiscard]] constexpr int exit_code(NexusEdgeDemoOutcome outcome) noexcept {
    return outcome == NexusEdgeDemoOutcome::COMPLETED ? 0 : 1;
}

/// Every configuration key the demo understands (runtime, link, demo and the remote device keys), for the loader allow-list so
/// that typos are rejected.
[[nodiscard]] std::vector<std::string> nexus_edge_demo_config_keys();

/// The KOS-I4 reference application. It goes only through the production path:
///
///   Application -> RuntimeHost -> DeviceManager -> RemoteDevice -> RemoteEndpoint -> RemoteNode -> LoopbackLink
///               -> SimulatedTransport -> EdgeHost -> I3 Device / Endpoint -> mock hardware
///
/// and never uses the session layer directly. Software only: Linux host, simulation, no hardware, no wall clock, no threads, no
/// sleeping: the same configuration always produces the same output. The Edge is the actuator-safety authority.
///
/// Scenario (every step is an expectation; the first failure shuts the runtime down and ends with ERROR):
///   1. compose the Edge (mock IMU and motor, EdgeHost) and the Nexus (RemoteNode, DeviceManager, RuntimeHost); the link timing is
///      read from the configuration first (it is proposed in HELLO), the node connects and discovers the topology, the whole
///      configuration is validated against the allow-list and given to the RuntimeHost;
///   2. initialize and start through the RuntimeHost (a fresh session, one request per endpoint);
///   3. use the endpoints through the unchanged I3 interfaces: read the sensors and the position, write a motor command, which the
///      Edge validates against its own limits; observe through the runtime and the passive diagnostics of both sides;
///   4. run on with both sides driven by the LoopbackLink and watch the heartbeats;
///   5. (demo.hostile_link) duplicate and replay traffic on the link: a duplicated write is applied once, a replayed old write is
///      never applied, duplicated answers are rejected;
///   6. lose the link: the Nexus faults its live remote endpoints (one ERROR each, one link record, reason "link lost"), the Edge
///      stops its actuator by its own heartbeat timeout and keeps its sensors; the two views differ until both are driven;
///   7. nothing recovers by itself: more time, observation and diagnostics change nothing;
///   8. recover explicitly through the runtime: stop, shutdown, initialize (a fresh HELLO and discovery), start; a second live
///      period works, the faults are cleared only by that lifecycle;
///   9. controlled shutdown, everything STOPPED.
///
/// Configuration keys: runtime.name (required); link.heartbeat_period_ms, link.heartbeat_timeout_ms, link.request_timeout_ms,
/// link.pump_quantum_ms; demo.link_latency_ms (0..50, default 1), demo.command_milli_rad_s (default 500), demo.run_ms (10..5000,
/// default 500), demo.hostile_link (bool, default true); and the remote device keys (`<device>.enabled`,
/// `<device>.<endpoint>.<setting>` for the declared settings).
class NexusEdgeDemoApplication {
public:
    explicit NexusEdgeDemoApplication(std::ostream& out) : out_(out) {}
    [[nodiscard]] NexusEdgeDemoOutcome run(const core::Configuration& configuration);

private:
    std::ostream& out_;
};

} // namespace kritva::demo
