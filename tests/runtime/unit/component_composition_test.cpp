//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : component_composition_test.cpp
// Description : Unit tests of component composition through RuntimeHost::add_component.
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-CMP-001..008; RR-DEP-001..005; RR-TST-003
// API         : RUNTIME-COMPOSITION-UT
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <string>
#include <vector>

#include "../check.hpp"
#include "../probe_component.hpp"

using namespace kritva;
using kritva::core::ErrorCode;
using kritva::core::LifecycleState;
using kritva::runtime::RuntimeHost;
using kritva::runtime::parse_configuration;
using kritva::runtime::test::ProbeComponent;
using Log = std::vector<std::string>;

static void test_registration_and_identity() {                       // RR-CMP-001/002
    RuntimeHost host;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log);
    ProbeComponent dup(1, "other", host.runtime(), log);             // same identity
    KRITVA_CHECK(host.add_component(a).has_value());
    KRITVA_CHECK(host.runtime().registry().find(a.info().id()) == &a);
    const auto r = host.add_component(dup);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(host.runtime().registry().find(a.info().id()) == &a);   // original untouched
}

static void test_dependency_order() {                                // RR-CMP-004/005, RR-DEP-002
    RuntimeHost host;
    Log log;
    ProbeComponent c(3, "c", host.runtime(), log);                   // c <- b <- a (a depends on b, b on c)
    ProbeComponent b(2, "b", host.runtime(), log);
    ProbeComponent a(1, "a", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a, {b.info().id()}).has_value());   // registration order is irrelevant
    KRITVA_CHECK(host.add_component(b, {c.info().id()}).has_value());
    KRITVA_CHECK(host.add_component(c).has_value());

    const auto order = host.runtime().component_order();
    KRITVA_CHECK(order.has_value() && order.value().size() == 3);
    KRITVA_CHECK(order.value()[0] == c.info().id() && order.value()[1] == b.info().id() && order.value()[2] == a.info().id());

    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.run().has_value());
    const Log expected{"c.initialize:INITIALIZING", "b.initialize:INITIALIZING", "a.initialize:INITIALIZING",
                       "c.start:READY", "b.start:READY", "a.start:READY",
                       "a.stop:STOPPING", "b.stop:STOPPING", "c.stop:STOPPING",
                       "a.shutdown:STOPPED", "b.shutdown:STOPPED", "c.shutdown:STOPPED"};
    KRITVA_CHECK(log == expected);
}

static void test_missing_dependency() {                              // RR-DEP-003
    RuntimeHost host;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a, {core::runtime::ComponentId{99}}).has_value());   // not registered
    // Core validates the topology when configuration is applied and again at initialize().
    const auto cfg = parse_configuration("runtime.name=test\n").value();
    auto r = host.configure(cfg);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::CONFIGURATION_ERROR);
    r = host.initialize();                                           // never configured: refused as well
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::CONFIGURATION_ERROR);
    KRITVA_CHECK(host.state() == LifecycleState::UNKNOWN);           // never READY
    KRITVA_CHECK(log.empty());                                       // no component invoked
    KRITVA_CHECK(!host.run().has_value());                           // run reports a failure, cleanly
    KRITVA_CHECK(log.empty());
}

static void test_cyclic_dependency_rejected() {                      // RR-DEP-005
    RuntimeHost host;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log);
    ProbeComponent b(2, "b", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a, {b.info().id()}).has_value());
    const auto r = host.add_component(b, {a.info().id()});           // closes a -> b -> a
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(host.runtime().registry().find(b.info().id()) == &b);   // documented: registered, dependency refused
    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.run().has_value());                            // graph stayed acyclic and runnable
}

static void test_invalid_dependencies() {
    RuntimeHost host;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log);
    ProbeComponent b(2, "b", host.runtime(), log);
    KRITVA_CHECK(!host.add_component(a, {a.info().id()}).has_value());   // self dependency
    KRITVA_CHECK(!host.add_component(b, {a.info().id(), a.info().id()}).has_value());   // duplicate edge
}

static void test_topology_fixed_after_initialize() {
    RuntimeHost host;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log);
    ProbeComponent late(2, "late", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a).has_value());
    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.initialize().has_value());
    const auto r = host.add_component(late);
    KRITVA_CHECK(!r.has_value() && r.error().code == ErrorCode::INVALID_STATE);
}

static void test_component_lifecycle_state() {                       // RR-CMP-006
    RuntimeHost host;
    Log log;
    ProbeComponent a(1, "a", host.runtime(), log);
    KRITVA_CHECK(host.add_component(a).has_value());
    KRITVA_CHECK(a.lifecycle_state() == LifecycleState::UNKNOWN);
    kritva::runtime::test::configure_host(host);
    KRITVA_CHECK(host.initialize().has_value() && a.lifecycle_state() == LifecycleState::READY);
    KRITVA_CHECK(host.start().has_value() && a.lifecycle_state() == LifecycleState::RUNNING);
    KRITVA_CHECK(host.stop().has_value() && a.lifecycle_state() == LifecycleState::STOPPED);
}

int main() {
    test_registration_and_identity();
    test_dependency_order();
    test_missing_dependency();
    test_cyclic_dependency_rejected();
    test_invalid_dependencies();
    test_topology_fixed_after_initialize();
    test_component_lifecycle_state();
    std::printf("component_composition_test: PASS\n");
    return 0;
}
