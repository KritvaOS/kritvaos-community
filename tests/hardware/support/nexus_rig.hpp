//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : nexus_rig.hpp
// Description : Test rig: a RemoteNode (Nexus) and an EdgeHost over mock devices on one simulated link, with
//               the peer tick wired to the Edge so synchronous calls run in one process.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: RR-001; RR-002
// API         : NexusRig
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <functional>

#include "edge_rig.hpp"

#include <kritva/hardware/remote/remote_endpoint.hpp>
#include <kritva/hardware/remote/remote_node.hpp>

namespace kritva::hardware::remote::test {

inline RemoteSettings mock_settings() {
    return {{{"motor", "command"}, {"min_rad_s", "max_rad_s", "fail_after_writes", "fault_after_writes"}},
            {{"imu", "acceleration"}, {"start", "step", "fail_after_ops", "fault_after_ops"}}};
}

class NexusRig {
public:
    EdgeRig edge_rig;                                   // the Edge side: link, mock devices, registry, EdgeHost
    std::function<void()> before_edge;                  // a test hook run before the Edge handles what is due
    EdgeHost* active_edge{nullptr};                     // which EdgeHost the peer tick drives
    RemoteNode node;

    explicit NexusRig(transport::LinkTiming timing = {}, RemoteSettings settings = mock_settings(), bool with_devices = true)
        : edge_rig(with_devices), active_edge(edge_rig.edge.get()),
          node(edge_rig.link.nexus(), make_config(timing), std::move(settings)) {}

    [[nodiscard]] SimulatedTransport& link() { return edge_rig.link; }
    [[nodiscard]] EdgeHost& edge() { return *active_edge; }
    [[nodiscard]] RemoteSession& session() { return node.session(); }
    [[nodiscard]] mock::MockImuDevice& imu() { return edge_rig.imu; }
    [[nodiscard]] mock::MockMotorDevice& motor() { return edge_rig.motor; }

    RemoteDevice& remote_device(const char* name) {
        for (const auto& d : node.devices()) if (d->info().name() == name) return *d;
        std::fprintf(stderr, "no remote device %s\n", name);
        std::exit(1);
    }
    template <class T>
    T& remote(const char* device, const char* endpoint) {
        Endpoint* e = remote_device(device).find_endpoint(endpoint);
        KRITVA_CHECK(e != nullptr);
        T* t = dynamic_cast<T*>(e);
        KRITVA_CHECK(t != nullptr);
        return *t;
    }

    // connect, then bring every remote endpoint to RUNNING through the I3 endpoint API (no DeviceManager).
    void connect_and_run() {
        KRITVA_CHECK(node.connect().has_value());
        for (const auto& d : node.devices()) {
            for (Endpoint* e : d->endpoints()) {
                KRITVA_CHECK(e->configure(core::Configuration{}).has_value());
                KRITVA_CHECK(e->initialize().has_value());
                KRITVA_CHECK(e->start().has_value());
            }
        }
    }

private:
    RemoteSessionConfig make_config(transport::LinkTiming timing) {
        RemoteSessionConfig c;
        c.nexus_node = NodeId{kNexusNode};
        c.edge_node = NodeId{kEdgeNode};
        c.timing = timing;
        c.peer_tick = [this] {
            if (before_edge) before_edge();
            if (active_edge != nullptr) active_edge->poll();
        };
        return c;
    }
};

// A raw frame from the Edge side into the Nexus (a crafted answer or notice).
inline void inject_to_nexus(SimulatedTransport& link, MessageType type, const Bytes& payload, std::uint64_t sequence,
                            std::uint64_t session, std::uint64_t correlation) {
    KRITVA_CHECK(link.edge().send(EdgeRig::frame(type, payload, sequence, session, correlation)).has_value());
}

} // namespace kritva::hardware::remote::test
