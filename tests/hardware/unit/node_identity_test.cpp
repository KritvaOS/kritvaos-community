//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : node_identity_test.cpp
// Description : Unit tests of NodeId, SessionId and EndpointAddress.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: NDR-001; NDR-002; NDR-003
// API         : NODE-IDENTITY-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <map>
#include <set>
#include <type_traits>

#include "../../runtime/check.hpp"

#include <kritva/hardware/remote/node.hpp>

using namespace kritva::hardware;
using namespace kritva::hardware::remote;

static_assert(!std::is_convertible_v<NodeId, DeviceId> && !std::is_convertible_v<DeviceId, NodeId>);
static_assert(!std::is_convertible_v<NodeId, SessionId> && !std::is_convertible_v<SessionId, NodeId>);
static_assert(!std::is_convertible_v<EndpointId, NodeId> && !std::is_convertible_v<std::uint64_t, NodeId>);

constexpr EndpointAddress addr(std::uint64_t node, std::uint64_t device, std::uint64_t endpoint) {
    return EndpointAddress{NodeId{node}, DeviceId{device}, EndpointId{endpoint}};
}

static void test_strong_ids() {                                          // NDR-001
    KRITVA_CHECK(!NodeId{}.valid() && !NodeId{0}.valid() && NodeId{1}.valid() && NodeId{7}.value() == 7);
    KRITVA_CHECK(!SessionId{}.valid() && SessionId{1}.valid() && SessionId{3} < SessionId{4} && SessionId{5} == SessionId{5});
    KRITVA_CHECK(NodeId{1} != NodeId{2});
}

static void test_address() {
    const EndpointAddress a{NodeId{1}, DeviceId{2}, EndpointId{3}};
    KRITVA_CHECK(a.valid() && a == addr(1, 2, 3));
    KRITVA_CHECK(!EndpointAddress{}.valid());
    KRITVA_CHECK(!addr(0, 2, 3).valid());       // each part is required
    KRITVA_CHECK(!addr(1, 0, 3).valid());
    KRITVA_CHECK(!addr(1, 2, 0).valid());
    KRITVA_CHECK(a != addr(9, 2, 3) && a != addr(1, 9, 3) &&
                 a != addr(1, 2, 9));
}

static void test_ids_may_collide_across_nodes() {                        // NDR-002
    const EndpointAddress on_a{NodeId{1}, DeviceId{10}, EndpointId{1}};
    const EndpointAddress on_b{NodeId{2}, DeviceId{10}, EndpointId{1}};                 // same device and endpoint ids on another node
    KRITVA_CHECK(on_a != on_b && on_a.device == on_b.device && on_a.endpoint == on_b.endpoint);
    std::map<EndpointAddress, int> registry;                                            // a registry keyed by address keeps both
    registry[on_a] = 1;
    registry[on_b] = 2;
    KRITVA_CHECK(registry.size() == 2 && registry.at(on_a) == 1 && registry.at(on_b) == 2);
}

static void test_ordering_is_deterministic() {
    std::set<EndpointAddress> ordered{{NodeId{2}, DeviceId{1}, EndpointId{1}}, {NodeId{1}, DeviceId{2}, EndpointId{1}},
                                      {NodeId{1}, DeviceId{1}, EndpointId{2}}, {NodeId{1}, DeviceId{1}, EndpointId{1}}};
    auto it = ordered.begin();
    KRITVA_CHECK(*it++ == EndpointAddress(NodeId{1}, DeviceId{1}, EndpointId{1}));      // node, then device, then endpoint
    KRITVA_CHECK(*it++ == EndpointAddress(NodeId{1}, DeviceId{1}, EndpointId{2}));
    KRITVA_CHECK(*it++ == EndpointAddress(NodeId{1}, DeviceId{2}, EndpointId{1}));
    KRITVA_CHECK(*it++ == EndpointAddress(NodeId{2}, DeviceId{1}, EndpointId{1}));
}

static void test_names_are_not_part_of_an_address() {
    static_assert(sizeof(EndpointAddress) == 3 * sizeof(std::uint64_t), "an address is exactly three ids");
    static_assert(std::is_trivially_copyable_v<EndpointAddress>);
}

int main() {
    test_strong_ids();
    test_address();
    test_ids_may_collide_across_nodes();
    test_ordering_is_deterministic();
    test_names_are_not_part_of_an_address();
    std::printf("node_identity_test: PASS\n");
    return 0;
}
