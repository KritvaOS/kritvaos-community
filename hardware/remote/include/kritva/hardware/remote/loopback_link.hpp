//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : loopback_link.hpp
// Description : The explicit same-process driver: spends virtual time on a simulated link while driving both sides'
//               supervision in a fixed, documented order.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: RI-001; RR-002; TR-001
// API         : kritva::hardware::remote::LoopbackLink
//
// Author      : KritvaOS
// Created     : 06-10-2026
//==============================================================================

#pragma once

#include <cstdint>
#include <functional>

#include <kritva/core/core.hpp>
#include <kritva/hardware/remote/edge_host.hpp>
#include <kritva/hardware/remote/remote_node.hpp>
#include <kritva/hardware/transport/simulated_transport.hpp>

namespace kritva::hardware::remote {

/// A composition utility for a Nexus and an Edge in one process over a SimulatedTransport. It is not a Core Component,
/// not a Device or an Endpoint and not a manager: it owns nothing (not the transport, the node, the Edge, a RuntimeHost or a
/// DeviceManager), has no clock of its own, no thread, no sleep and no wall clock, and makes no lifecycle, safety, reconnect
/// or recovery decision. It only makes the driving responsibility visible at the call site:
///
///   advance(dt):  repeat until dt of virtual time is consumed:
///                   1. the transport's clock moves by min(quantum, remaining);
///                   2. EdgeHost::poll()      - the Edge handles what is due, then supervises;
///                   3. RemoteNode::service() - the Nexus reads what is due (the Edge's answers included), then supervises.
///
/// The order is fixed and deterministic: the same script of advance() calls over the same link configuration always produces
/// the same behavior. Supervision happens only inside such calls (and inside the Nexus's synchronous request pump, which uses
/// peer_tick()): a gap in driving is a gap in supervision, and nothing here hides it.
class LoopbackLink {
public:
    /// The default quantum is the protocol's `link.pump_quantum_ms` default (1 ms).
    static constexpr std::uint64_t kDefaultQuantumNs = 1'000'000;

    LoopbackLink(transport::SimulatedTransport& transport, EdgeHost& edge, std::uint64_t quantum_ns = kDefaultQuantumNs) noexcept
        : transport_(transport), edge_(edge), quantum_ns_(quantum_ns == 0 ? kDefaultQuantumNs : quantum_ns) {}
    LoopbackLink(const LoopbackLink&) = delete;
    LoopbackLink& operator=(const LoopbackLink&) = delete;

    /// The peer tick for the Nexus's RemoteSessionConfig: it lets the Edge handle what is due while a synchronous request waits.
    [[nodiscard]] std::function<void()> peer_tick() { return [this] { edge_.poll(); }; }

    /// Connects the Nexus end that advance() supervises (the node is created with peer_tick() in its configuration, so it
    /// exists after this object). Without it advance() drives only the Edge.
    void attach(RemoteNode& nexus) noexcept { nexus_ = &nexus; }

    /// Spends `duration_ns` of virtual time as described above. INVALID_ARGUMENT if the clock would overflow (nothing is
    /// consumed beyond what was already done).
    core::Result<void> advance(std::uint64_t duration_ns);

    /// One round at the current time without moving the clock: the Edge polls, then the Nexus is serviced.
    void pump();

    [[nodiscard]] std::uint64_t quantum_ns() const noexcept { return quantum_ns_; }
    [[nodiscard]] std::uint64_t now_ns() const noexcept { return transport_.now_ns(); }

private:
    transport::SimulatedTransport& transport_;
    EdgeHost& edge_;
    RemoteNode* nexus_{nullptr};
    std::uint64_t quantum_ns_;
};

} // namespace kritva::hardware::remote
