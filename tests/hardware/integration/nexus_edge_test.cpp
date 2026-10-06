//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : nexus_edge_test.cpp
// Description : Integration of the Nexus proxies with the EdgeHost over the simulated transport: latency, and a
//               seeded fault campaign (drop, duplicate, reorder) checking that no command is ever applied twice.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: RR-002; SR-002 (mechanism); TR-002 (HUMAN SAFETY REVIEW OPEN)
// API         : NEXUS-EDGE-IT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <cmath>
#include <set>

#include "../support/nexus_rig.hpp"

using namespace kritva::hardware::remote;
using namespace kritva::hardware::remote::test;
using kritva::core::ErrorCode;
namespace hw = kritva::hardware;

static constexpr std::uint64_t kMs = 1'000'000;

static void test_a_full_cycle_over_a_link_with_latency() {
    NexusRig rig;
    DirectionConfig c;
    c.latency_ns = 3 * kMs;                                                  // 3 ms each way
    KRITVA_CHECK(rig.link().configure(LinkDirection::NEXUS_TO_EDGE, c).has_value() && rig.link().configure(LinkDirection::EDGE_TO_NEXUS, c).has_value());
    const auto t0 = rig.link().now_ns();
    KRITVA_CHECK(rig.node.connect().has_value());
    // HELLO and discovery: two round trips of 6 ms, each completed on a 1 ms pump quantum.
    KRITVA_CHECK(rig.link().now_ns() - t0 == 12 * kMs);
    for (const auto& d : rig.node.devices()) for (hw::Endpoint* e : d->endpoints()) KRITVA_CHECK(e->initialize().has_value() && e->start().has_value());
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    auto& pos = rig.remote<RemotePositionEndpoint>("motor", "position");
    const auto before = rig.link().now_ns();
    KRITVA_CHECK(cmd.write(hw::MotorCommand{0.5}).has_value());
    KRITVA_CHECK(rig.link().now_ns() - before == 6 * kMs);                  // one round trip
    hw::PositionSample p;
    KRITVA_CHECK(pos.read(p).has_value() && rig.motor().model().velocity_rad_s == 0.5);
    KRITVA_CHECK(rig.session().stats().timeouts == 0 && rig.session().stats().late_responses == 0);
}

// The safety property: whatever the link does, a command that the Nexus wrote once is applied by the Edge at most
// once, and a stale one is never applied after a newer one.
struct Outcome {
    std::uint64_t writes_called{0}, writes_ok{0}, timeouts{0}, retransmits{0}, applied{0}, resent{0}, stale{0};
    std::vector<double> applied_values;
};

static Outcome campaign(std::uint64_t seed, int writes) {
    NexusRig rig;
    rig.connect_and_run();
    auto& cmd = rig.remote<RemoteMotorCommandEndpoint>("motor", "command");
    DirectionConfig c;
    c.latency_ns = 2 * kMs;
    c.seed = seed;
    c.drop_permille = 200;
    c.duplicate_permille = 250;
    c.reorder_permille = 250;
    c.reorder_delay_ns = 30 * kMs;
    KRITVA_CHECK(rig.link().configure(LinkDirection::NEXUS_TO_EDGE, c).has_value());
    c.seed = seed + 1000;
    KRITVA_CHECK(rig.link().configure(LinkDirection::EDGE_TO_NEXUS, c).has_value());

    Outcome out;
    double last_seen = rig.motor().model().velocity_rad_s;
    std::set<double> applied_set;
    const auto note_application = [&] {
        const double v = rig.motor().model().velocity_rad_s;
        if (v == 0.0) { last_seen = v; return; }                              // a stop to zero is the Edge's safety action, not an application
        if (v != last_seen) {
            if (applied_set.count(v) != 0) {                                 // a value applied a second time
                std::fprintf(stderr, "value %f applied twice (seed %llu)\n", v, static_cast<unsigned long long>(seed));
                std::exit(1);
            }
            applied_set.insert(v);
            out.applied_values.push_back(v);
            last_seen = v;
        }
    };
    for (int i = 1; i <= writes; ++i) {
        // A session the supervision has legitimately ended (every heartbeat of a window lost) ends the campaign: from then on
        // the proxy is FAULT and the Edge has stopped its actuator, which is the safe outcome, not an application.
        if (cmd.lifecycle_state() != kritva::core::LifecycleState::RUNNING || !rig.edge().session().valid()) break;
        const double v = 0.001 * i;                                          // every command has its own value, within the Edge limits
        ++out.writes_called;
        auto r = cmd.write(hw::MotorCommand{v});
        note_application();
        if (r.has_value()) { ++out.writes_ok; }
        else if (r.error().code == ErrorCode::TIMEOUT) {
            ++out.timeouts;
            if (i % 3 == 0) {                                                // sometimes the application retransmits explicitly
                ++out.retransmits;
                (void)cmd.retransmit_last_write();
                note_application();
            }
        }
        // Let late, duplicated and reordered frames of earlier requests arrive between calls, with both sides driven (supervision
        // runs only when driven).
        for (int step = 0; step < 4; ++step) {
            KRITVA_CHECK(rig.link().advance(10 * kMs).has_value());
            rig.node.service();
            rig.edge().poll();
            note_application();
        }
        if (i % 5 == 0) { (void)rig.session().observe(EndpointAddress{NodeId{kEdgeNode}, hw::DeviceId{kMotor}, hw::EndpointId{kCommand}}); }
    }
    const EdgeStats& e = rig.edge().stats();
    out.applied = e.writes_applied;
    out.resent = e.writes_resent;
    out.stale = e.stale_frames;
    // Every application is counted once by the Edge endpoint itself: the I3 write counter equals the ledger's applications.
    KRITVA_CHECK(rig.motor().command().statistics().sample_count.value() == out.applied);
    KRITVA_CHECK(out.applied >= out.applied_values.size());
    KRITVA_CHECK(out.applied <= out.writes_called);                          // never more applications than writes the Nexus made
    // The values applied appear in the order they were written (a stale command is never applied after a newer one).
    for (std::size_t k = 1; k < out.applied_values.size(); ++k) KRITVA_CHECK(out.applied_values[k] > out.applied_values[k - 1]);
    return out;
}

static void test_a_fault_campaign_never_applies_a_command_twice() {
    std::uint64_t total_timeouts = 0, total_resent = 0, total_stale = 0;
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        const Outcome o = campaign(seed, 60);
        total_timeouts += o.timeouts;
        total_resent += o.resent;
        total_stale += o.stale;
    }
    // The campaign really exercised the faults (otherwise it proves little).
    KRITVA_CHECK(total_timeouts > 50 && total_stale > 25);
    KRITVA_CHECK(total_resent > 0);
}

static void test_the_campaign_is_repeatable() {
    const Outcome a = campaign(7, 80);
    const Outcome b = campaign(7, 80);
    KRITVA_CHECK(a.applied_values == b.applied_values && a.timeouts == b.timeouts && a.applied == b.applied && a.resent == b.resent && a.stale == b.stale);
    KRITVA_CHECK(campaign(8, 80).applied_values != a.applied_values);
}

int main() {
    test_a_full_cycle_over_a_link_with_latency();
    test_a_fault_campaign_never_applies_a_command_twice();
    test_the_campaign_is_repeatable();
    std::printf("nexus_edge_test: PASS\n");
    return 0;
}
