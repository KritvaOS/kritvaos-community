//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : topology_test.cpp
// Description : Unit tests of the topology identity tuples, the typed-mapping check and set equivalence.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: ER-002; RR-001
// API         : TOPOLOGY-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include "../../runtime/check.hpp"

#include <kritva/hardware/remote/topology.hpp>

using namespace kritva::hardware::remote;
using namespace kritva::hardware::transport;
using kritva::core::ErrorCode;

static DiscoveredEndpoint ep(std::uint64_t id, const char* name, WireDirection d, std::uint64_t cap) {
    return DiscoveredEndpoint{id, name, d, {DiscoveredCapability{cap, "c"}}};
}
static DiscoveryResponsePayload sample() {
    DiscoveryResponsePayload p;
    p.devices = {
        DiscoveredDevice{1, "imu", {ep(1, "acceleration", WireDirection::SENSOR, 0x1001), ep(2, "angular_velocity", WireDirection::SENSOR, 0x1002)}},
        DiscoveredDevice{2, "motor", {ep(1, "command", WireDirection::ACTUATOR, 0x2001), ep(2, "position", WireDirection::SENSOR, 0x1003)}},
    };
    return p;
}
static Topology topo(const DiscoveryResponsePayload& p, std::uint64_t node = 7) { return topology_of(NodeId{node}, p); }

static void test_tuples() {
    const Topology t = topo(sample());
    KRITVA_CHECK(t.size() == 4);
    KRITVA_CHECK(t[0].node == 7 && t[0].device_id == 1 && t[0].device_name == "imu" && t[0].endpoint_id == 1 && t[0].endpoint_name == "acceleration" &&
                 t[0].direction == WireDirection::SENSOR && t[0].capability_ids == std::vector<std::uint64_t>{0x1001});
    KRITVA_CHECK(t[2].direction == WireDirection::ACTUATOR && t[2].capability_ids[0] == 0x2001);
    KRITVA_CHECK(topology_of(NodeId{1}, DiscoveryResponsePayload{}).empty());
}

static void test_equivalence_ignores_order() {
    const auto a = sample();
    auto b = sample();
    std::swap(b.devices[0], b.devices[1]);                                     // devices in another order
    std::swap(b.devices[0].endpoints[0], b.devices[0].endpoints[1]);           // endpoints in another order
    KRITVA_CHECK(topology_equivalent(topo(a), topo(b)));
    KRITVA_CHECK(topology_equivalent(topo(a), topo(a)));
    KRITVA_CHECK(topology_equivalent({}, {}));
}

// Every element of the identity tuple matters.
static void test_equivalence_detects_every_difference() {
    const Topology base = topo(sample());
    const auto differs = [&](DiscoveryResponsePayload changed, std::uint64_t node = 7) { return !topology_equivalent(base, topo(changed, node)); };
    { auto p = sample(); p.devices[0].id = 9; KRITVA_CHECK(differs(p)); }                                  // device id
    { auto p = sample(); p.devices[0].name = "imu2"; KRITVA_CHECK(differs(p)); }                           // device name
    { auto p = sample(); p.devices[0].endpoints[0].id = 9; KRITVA_CHECK(differs(p)); }                     // endpoint id
    { auto p = sample(); p.devices[0].endpoints[0].name = "accel"; KRITVA_CHECK(differs(p)); }             // endpoint name
    { auto p = sample(); p.devices[1].endpoints[0].direction = WireDirection::SENSOR; KRITVA_CHECK(differs(p)); }   // direction
    { auto p = sample(); p.devices[0].endpoints[0].capabilities[0].id = 0x1002; KRITVA_CHECK(differs(p)); }        // capability id
    { auto p = sample(); p.devices[0].endpoints.pop_back(); KRITVA_CHECK(differs(p)); }                    // removed endpoint
    { auto p = sample(); p.devices.pop_back(); KRITVA_CHECK(differs(p)); }                                 // removed device
    { auto p = sample(); p.devices.push_back(DiscoveredDevice{3, "extra", {ep(1, "e", WireDirection::SENSOR, 0x1001)}}); KRITVA_CHECK(differs(p)); }   // added device
    { auto p = sample(); p.devices[0].endpoints.push_back(ep(3, "extra", WireDirection::SENSOR, 0x1001)); KRITVA_CHECK(differs(p)); }              // added endpoint
    KRITVA_CHECK(differs(sample(), 8));                                                                    // node id
    // A move of an endpoint to another device is a change.
    { auto p = sample(); auto moved = p.devices[0].endpoints.back(); p.devices[0].endpoints.pop_back(); p.devices[1].endpoints.push_back(moved); KRITVA_CHECK(differs(p)); }
    // A permutation that keeps every tuple is not.
    KRITVA_CHECK(!differs(sample()));
}

static void test_capability_order_inside_an_endpoint_is_not_identity() {
    auto a = sample();
    a.devices[0].endpoints[0].capabilities.push_back(DiscoveredCapability{0x1003, "p"});
    auto b = a;
    std::swap(b.devices[0].endpoints[0].capabilities[0], b.devices[0].endpoints[0].capabilities[1]);
    KRITVA_CHECK(topology_equivalent(topo(a), topo(b)));
}

static void test_typed_mapping_check() {
    KRITVA_CHECK(check_supported(sample()).has_value());
    KRITVA_CHECK(check_supported(DiscoveryResponsePayload{}).has_value());
    const auto bad = [](const auto& mutate) {
        auto p = sample();
        mutate(p);
        const auto r = check_supported(p);
        return !r.has_value() && r.error().code == ErrorCode::UNSUPPORTED;
    };
    KRITVA_CHECK(bad([](auto& p) { p.devices[0].endpoints[0].capabilities[0].id = 0x1004; }));                       // unknown id
    KRITVA_CHECK(bad([](auto& p) { p.devices[0].endpoints[0].capabilities[0].id = 0x2001; }));                       // actuator id on a sensor
    KRITVA_CHECK(bad([](auto& p) { p.devices[1].endpoints[0].capabilities[0].id = 0x1001; }));                       // sensor id on an actuator
    KRITVA_CHECK(bad([](auto& p) { p.devices[0].endpoints[0].capabilities.clear(); }));                              // none
    KRITVA_CHECK(bad([](auto& p) { p.devices[0].endpoints[0].capabilities.push_back(DiscoveredCapability{0x1002, "x"}); }));   // two
    KRITVA_CHECK(bad([](auto& p) { p.devices[1].endpoints[0].capabilities[0].id = 0x1003; }));
    KRITVA_CHECK(bad([](auto& p) { p.devices[0].endpoints[0].capabilities[0].id = 0; }));
}

int main() {
    test_tuples();
    test_equivalence_ignores_order();
    test_equivalence_detects_every_difference();
    test_capability_order_inside_an_endpoint_is_not_identity();
    test_typed_mapping_check();
    std::printf("topology_test: PASS\n");
    return 0;
}
