//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : stack_rig.hpp
// Description : Test rig of the whole production path: RuntimeHost, DeviceManager, remote devices, LoopbackLink, simulated
//               transport, EdgeHost and mock devices.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: RI-001; RI-002; RI-003; DR-001
// API         : StackRig
//
// Author      : KritvaOS
// Created     : 06-10-2026
//==============================================================================

#pragma once

#include <memory>
#include <string>

#include "edge_rig.hpp"
#include "nexus_rig.hpp"

#include <kritva/hardware/device_manager.hpp>
#include <kritva/hardware/remote/edge_diagnostics.hpp>
#include <kritva/hardware/transport/link_config.hpp>
#include <kritva/hardware/remote/loopback_link.hpp>
#include <kritva/hardware/remote/remote_diagnostics.hpp>
#include <kritva/runtime/runtime_host.hpp>

namespace kritva::hardware::remote::test {

// Application -> RuntimeHost -> DeviceManager -> RemoteDevice -> RemoteEndpoint -> RemoteNode -> LoopbackLink -> SimulatedTransport
// -> EdgeHost -> I3 Device/Endpoint -> mock. The Edge is driven only by the LoopbackLink (explicitly) and by the Nexus's synchronous pump.
class StackRig {
public:
    EdgeRig edge_rig;                                        // the Edge side: transport, mock devices, registry, EdgeHost
    LoopbackLink loop;
    std::unique_ptr<RemoteNode> node;
    DeviceManager manager{kritva::core::runtime::ComponentId{300}};
    runtime::RuntimeHost host;
    runtime::EventLog events;

    // The link timing is read first (it is proposed in HELLO), then the node connects and discovers, then the whole
    // configuration is validated against the allow-list (the manager's keys exist only after discovery) and given to the host.
    explicit StackRig(const std::string& config_text = "runtime.name=stack\n")
        : edge_rig(true), loop(edge_rig.link, *edge_rig.edge) {
        const auto first = runtime::parse_configuration(config_text);                  // no allow-list yet: only the link.* keys are read
        KRITVA_CHECK(first.has_value());
        const auto timing = link_timing_from(first.value());
        KRITVA_CHECK(timing.has_value());
        RemoteSessionConfig cfg;
        cfg.nexus_node = NodeId{kNexusNode};
        cfg.edge_node = NodeId{kEdgeNode};
        cfg.timing = timing.value();
        cfg.peer_tick = loop.peer_tick();
        node = std::make_unique<RemoteNode>(edge_rig.link.nexus(), std::move(cfg), mock_settings());
        loop.attach(*node);
        KRITVA_CHECK(node->connect().has_value());
        for (const auto& d : node->devices()) KRITVA_CHECK(manager.register_device(*d).has_value());
        manager.set_event_sink(&events);
        host.set_event_sink(&events);
        KRITVA_CHECK(host.add_component(manager).has_value());
        auto keys = manager.config_keys();
        for (const auto& k : link_config_keys()) keys.push_back(k);
        keys.push_back("runtime.name");
        const auto full = runtime::parse_configuration(config_text, keys);
        if (!full) { std::fprintf(stderr, "config: %s\n", full.error().message.c_str()); std::exit(1); }
        const auto configured = host.configure(full.value());
        if (!configured) { std::fprintf(stderr, "host.configure: %s\n", configured.error().message.c_str()); std::exit(1); }
    }

    void up() { KRITVA_CHECK(host.initialize().has_value() && host.start().has_value()); }

    [[nodiscard]] EdgeHost& edge() { return *edge_rig.edge; }
    [[nodiscard]] SimulatedTransport& link() { return edge_rig.link; }
    [[nodiscard]] mock::MockMotorDevice& motor() { return edge_rig.motor; }
    [[nodiscard]] mock::MockImuDevice& imu() { return edge_rig.imu; }
    [[nodiscard]] RemoteSession& session() { return node->session(); }

    template <class T>
    T& remote(const char* device, const char* endpoint) {
        for (const auto& d : node->devices()) {
            if (d->info().name() != device) continue;
            Endpoint* e = d->find_endpoint(endpoint);
            KRITVA_CHECK(e != nullptr);
            T* t = dynamic_cast<T*>(e);
            KRITVA_CHECK(t != nullptr);
            return *t;
        }
        std::fprintf(stderr, "no remote device %s\n", device);
        std::exit(1);
    }

    [[nodiscard]] std::size_t error_events() const {
        std::size_t n = 0;
        for (const auto& e : events.snapshot()) n += e.type == kritva::core::EventType::ERROR ? 1 : 0;
        return n;
    }
};

} // namespace kritva::hardware::remote::test
