//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : edge_write_test.cpp
// Description : Unit tests of the Edge actuator write path in the order of protocol section 12: session, address and
//               capability, duplicate, stale, state, I3 validation before apply, apply once, ledger.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: SR-001; SR-002; ER-003 (HUMAN SAFETY REVIEW OPEN)
// API         : EDGE-WRITE-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <cmath>

#include "../support/edge_rig.hpp"

using namespace kritva::hardware::remote;
using namespace kritva::hardware::remote::test;
using kritva::core::ErrorCode;
using kritva::core::LifecycleState;

static Bytes write_payload(double v, std::uint64_t device = kMotor, std::uint64_t endpoint = kCommand) {
    return must(encode(WriteRequestPayload{AddressPayload{kEdgeNode, device, endpoint}, v}));
}

// Raw write with an explicit sequence in the current session; the Nexus counter is not touched.
static std::vector<Reply> raw_write(EdgeRig& rig, std::uint64_t sequence, double v) {
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(v), sequence, rig.session));
    return rig.poll();
}

static std::uint64_t applied_count(EdgeRig& rig) { return rig.motor.command().statistics().sample_count.value(); }

static EdgeRig& running_rig(EdgeRig& rig) {
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    return rig;
}

// 2: address, direction, capability.
static void test_step2_address_direction_and_capability() {
    EdgeRig rig;
    running_rig(rig);
    auto s = rig.write(99, kCommand, 0.1);
    KRITVA_CHECK(s.status.code == ErrorCode::INVALID_ARGUMENT && s.status.message == "unknown device");
    s = rig.write(kMotor, 99, 0.1);
    KRITVA_CHECK(s.status.code == ErrorCode::INVALID_ARGUMENT && s.status.message == "unknown endpoint");
    const Reply r = rig.call(MessageType::WRITE_REQUEST, must(encode(WriteRequestPayload{AddressPayload{kEdgeNode + 1, kMotor, kCommand}, 0.1})));
    KRITVA_CHECK(decode<StatusPayload>(r.payload).value.status.message == "unknown node");
    s = rig.write(kMotor, kPosition, 0.1);                                  // a sensor
    KRITVA_CHECK(s.status.code == ErrorCode::INVALID_ARGUMENT && s.status.message == "the endpoint is not a motor command endpoint");
    s = rig.write(kImu, kAccel, 0.1);
    KRITVA_CHECK(s.status.code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(rig.motor.model().velocity_rad_s == 0.0 && applied_count(rig) == 0 && rig.stats().writes_applied == 0 && rig.stats().writes_rejected == 5);
}

// 5: the endpoint must be RUNNING.
static void test_step5_state() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    auto s = rig.write(kMotor, kCommand, 0.1);                              // UNKNOWN
    KRITVA_CHECK(s.status.code == ErrorCode::NOT_READY);
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kMotor, kCommand).status.ok());
    s = rig.write(kMotor, kCommand, 0.1);                                   // READY, not RUNNING
    KRITVA_CHECK(s.status.code == ErrorCode::NOT_READY);
    KRITVA_CHECK(rig.lifecycle(MessageType::START_REQUEST, kMotor, kCommand).status.ok());
    KRITVA_CHECK(rig.lifecycle(MessageType::STOP_REQUEST, kMotor, kCommand).status.ok());
    s = rig.write(kMotor, kCommand, 0.1);                                   // STOPPED
    KRITVA_CHECK(s.status.code == ErrorCode::NOT_READY);
    KRITVA_CHECK(rig.motor.model().velocity_rad_s == 0.0 && applied_count(rig) == 0);

    // A faulted endpoint: RESOURCE_UNAVAILABLE.
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kMotor, kCommand).status.ok() && rig.lifecycle(MessageType::START_REQUEST, kMotor, kCommand).status.ok());
    KRITVA_CHECK(rig.motor.command().inject_fault("hardware fault").has_value());
    s = rig.write(kMotor, kCommand, 0.1);
    KRITVA_CHECK(s.status.code == ErrorCode::RESOURCE_UNAVAILABLE);
    KRITVA_CHECK(rig.motor.model().velocity_rad_s == 0.0 && applied_count(rig) == 0 && rig.stats().writes_applied == 0);
}

// 6: the I3 endpoint validates the command against its limits before it applies anything.
static void test_step6_limits_are_validated_before_apply() {              // SR-001
    EdgeRig rig;
    running_rig(rig);                                                       // default limits +-1 rad/s
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok() && rig.motor.model().velocity_rad_s == 0.5);
    for (const double bad : {1.0000001, -1.0000001, 2.0, -50.0, 1e300, -1e300}) {
        const auto s = rig.write(kMotor, kCommand, bad);
        KRITVA_CHECK(s.status.code == ErrorCode::INVALID_ARGUMENT);          // the endpoint's own error
        KRITVA_CHECK(rig.motor.model().velocity_rad_s == 0.5);               // nothing applied, the previous command stands
    }
    KRITVA_CHECK(rig.write(kMotor, kCommand, 1.0).status.ok() && rig.motor.model().velocity_rad_s == 1.0);       // the limits themselves are valid
    KRITVA_CHECK(rig.write(kMotor, kCommand, -1.0).status.ok() && rig.motor.model().velocity_rad_s == -1.0);
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.0).status.ok() && rig.motor.model().velocity_rad_s == 0.0);
    KRITVA_CHECK(rig.stats().writes_applied == 4 && rig.stats().writes_rejected == 6);
    // The Edge uses ITS limits, not anything the Nexus says: tightening them on the Edge takes effect at once.
    KRITVA_CHECK(rig.lifecycle(MessageType::STOP_REQUEST, kMotor, kCommand).status.ok());
    const Reply c = rig.call(MessageType::CONFIGURE_REQUEST, must(encode(ConfigureRequestPayload{AddressPayload{kEdgeNode, kMotor, kCommand},
                                                                                       {{"max_rad_s", std::int64_t{0}}, {"min_rad_s", std::int64_t{0}}}})));
    KRITVA_CHECK(decode<LifecycleResponsePayload>(c.payload).value.status.ok());
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kMotor, kCommand).status.ok() && rig.lifecycle(MessageType::START_REQUEST, kMotor, kCommand).status.ok());
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.1).status.code == ErrorCode::INVALID_ARGUMENT);
}

// A non-finite velocity cannot be decoded, so it never reaches the endpoint.
static void test_non_finite_commands_never_reach_the_endpoint() {
    EdgeRig rig;
    running_rig(rig);
    const std::uint64_t ops = applied_count(rig);
    for (const std::uint64_t bits : {0x7FF8000000000000ull, 0x7FF0000000000000ull, 0xFFF0000000000000ull, 0x7FF8000000000001ull}) {   // NaN, +inf, -inf, NaN payload
        Bytes p = write_payload(0.5);
        for (int i = 0; i < 8; ++i) p[24 + i] = static_cast<std::uint8_t>(bits >> (8 * i));
        const std::uint64_t s = rig.send(MessageType::WRITE_REQUEST, p);
        const auto replies = rig.poll();
        KRITVA_CHECK(replies.size() == 1 && replies[0].header.type == MessageType::PROTOCOL_ERROR && replies[0].header.correlation_id == s);
        --rig.seq;
    }
    KRITVA_CHECK(applied_count(rig) == ops && rig.motor.model().velocity_rad_s == 0.0 && rig.stats().writes_applied == 0);
}

// 7: apply once, record the sequence, cache the response.
static void test_step7_apply_once_and_answer() {
    EdgeRig rig;
    running_rig(rig);
    const std::uint64_t before = applied_count(rig);
    const std::uint64_t s = rig.send(MessageType::WRITE_REQUEST, write_payload(0.25));
    const auto replies = rig.poll();
    KRITVA_CHECK(replies.size() == 1 && replies[0].header.type == MessageType::WRITE_RESPONSE && replies[0].header.correlation_id == s &&
                 replies[0].header.session_id == rig.session);
    KRITVA_CHECK(decode<StatusPayload>(replies[0].payload).value.status.ok());
    KRITVA_CHECK(rig.motor.model().velocity_rad_s == 0.25 && rig.motor.command().effective_velocity() == 0.25);
    KRITVA_CHECK(applied_count(rig) == before + 1);                         // exactly one I3 write
}

// 3: a duplicate of the last applied write gets the cached response and is not applied again.
static void test_step3_duplicate_of_the_last_write_is_answered_not_reapplied() {   // SR-002
    EdgeRig rig;
    running_rig(rig);
    const std::uint64_t s = rig.send(MessageType::WRITE_REQUEST, write_payload(0.25));
    const auto first = rig.poll();
    KRITVA_CHECK(first.size() == 1);
    const std::uint64_t ops = applied_count(rig);

    for (int i = 0; i < 3; ++i) {
        const auto again = raw_write(rig, s, 0.25);                          // the same sequence and payload
        KRITVA_CHECK(again.size() == 1 && again[0].header.correlation_id == s && again[0].payload == first[0].payload);
        KRITVA_CHECK(again[0].header.sequence > first[0].header.sequence);   // a fresh Edge frame number each time
    }
    KRITVA_CHECK(applied_count(rig) == ops && rig.stats().writes_resent == 3 && rig.stats().writes_applied == 1);

    // Still answered after other frames raised the watermark.
    KRITVA_CHECK(rig.discover().status.ok());
    rig.send(MessageType::HEARTBEAT, must(encode(HeartbeatPayload{1})));
    KRITVA_CHECK(rig.poll().empty());
    KRITVA_CHECK(raw_write(rig, s, 0.25).size() == 1 && applied_count(rig) == ops && rig.stats().writes_resent == 4);

    // The resend does not change the ledger: a later write is applied and then the older duplicate is stale.
    const std::uint64_t s2 = rig.send(MessageType::WRITE_REQUEST, write_payload(0.5));
    KRITVA_CHECK(rig.poll().size() == 1 && rig.motor.model().velocity_rad_s == 0.5);
    KRITVA_CHECK(raw_write(rig, s, 0.25).empty());                           // no longer the last applied: dropped
    KRITVA_CHECK(rig.motor.model().velocity_rad_s == 0.5 && applied_count(rig) == ops + 1 && rig.stats().writes_resent == 4);
    KRITVA_CHECK(raw_write(rig, s2, 0.5).size() == 1);                       // the new last write can be resent
}

// 3/4: the same sequence with another payload is not a duplicate; it is stale.
static void test_same_sequence_with_a_different_command_is_never_applied() {
    EdgeRig rig;
    running_rig(rig);
    const std::uint64_t s = rig.send(MessageType::WRITE_REQUEST, write_payload(0.25));
    KRITVA_CHECK(rig.poll().size() == 1);
    const std::uint64_t ops = applied_count(rig);
    KRITVA_CHECK(raw_write(rig, s, 0.75).empty());                           // an attacker or a bug re-uses the sequence
    KRITVA_CHECK(rig.motor.model().velocity_rad_s == 0.25 && applied_count(rig) == ops);
    // A different address with the same sequence is not the duplicate either.
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.25, kMotor, kPosition), s, rig.session));
    KRITVA_CHECK(rig.poll().empty() && rig.stats().stale_frames == 2);
}

// 4: stale and reordered writes are dropped silently and never applied.
static void test_step4_stale_and_reordered_writes() {                      // SR-002
    EdgeRig rig;
    running_rig(rig);
    const std::uint64_t base = rig.seq;
    // Reordered by the link: 5 arrives before 4.
    KRITVA_CHECK(raw_write(rig, base + 3, 0.3).size() == 1 && rig.motor.model().velocity_rad_s == 0.3);
    const std::uint64_t ops = applied_count(rig);
    KRITVA_CHECK(raw_write(rig, base + 2, 0.9).empty());                     // the earlier command arrives late
    KRITVA_CHECK(raw_write(rig, base + 1, 0.8).empty());
    KRITVA_CHECK(raw_write(rig, 1, 0.7).empty());
    KRITVA_CHECK(rig.motor.model().velocity_rad_s == 0.3 && applied_count(rig) == ops && rig.stats().stale_frames == 3);
    KRITVA_CHECK(rig.stats().writes_applied == 1);
    // A stale write with an invalid address is silent as well (no error response for a stale frame).
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.1, 99, 99), base + 2, rig.session));
    KRITVA_CHECK(rig.poll().empty());
    // Sequences may have gaps; the next higher one is applied.
    KRITVA_CHECK(raw_write(rig, base + 50, 0.4).size() == 1 && rig.motor.model().velocity_rad_s == 0.4);
}

// A rejected write is not recorded: it neither updates the ledger nor can it be resent as a duplicate.
static void test_a_rejected_write_is_not_recorded() {
    EdgeRig rig;
    running_rig(rig);
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
    const std::uint64_t good = rig.seq;
    const std::uint64_t bad = rig.send(MessageType::WRITE_REQUEST, write_payload(5.0));      // out of limits
    const auto r = rig.poll();
    KRITVA_CHECK(r.size() == 1 && decode<StatusPayload>(r[0].payload).value.status.code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(raw_write(rig, bad, 5.0).empty());                          // a retransmission of the rejected one: stale, not answered again
    KRITVA_CHECK(rig.stats().writes_resent == 0);
    // The rejected write left the ledger alone, so the earlier applied write is still the last applied one: its duplicate is answered from the cache.
    const std::uint64_t ops = applied_count(rig);
    KRITVA_CHECK(raw_write(rig, good, 0.5).size() == 1 && rig.stats().writes_resent == 1 && applied_count(rig) == ops && rig.motor.model().velocity_rad_s == 0.5);
}

// The ledger is per endpoint: the last applied write of one actuator does not shadow another.
static void test_the_ledger_is_per_endpoint() {
    EdgeRig rig;
    running_rig(rig);
    // A second motor device on the same Edge is not available in the rig; the per-endpoint key is still observable:
    // a duplicate for the position sensor (not an actuator) never finds a ledger and is stale.
    const std::uint64_t s = rig.send(MessageType::WRITE_REQUEST, write_payload(0.25));
    KRITVA_CHECK(rig.poll().size() == 1);
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.25, kMotor, kPosition), s, rig.session));
    KRITVA_CHECK(rig.poll().empty() && rig.stats().writes_resent == 0 && rig.motor.model().velocity_rad_s == 0.25);
}

static void test_each_actuator_endpoint_has_its_own_ledger() {            // SR-002: keyed by device and endpoint
    using namespace kritva::hardware;
    class Twin final : public Device {
    public:
        Twin() : Device(DeviceInfo::create(DeviceId{6}, "twin").value()),
                 first_model_(std::make_shared<mock::MotorModel>()), second_model_(std::make_shared<mock::MotorModel>()) {
            auto a = std::make_unique<mock::MockMotorCommandEndpoint>(first_model_, EndpointId{1}, "first");
            auto b = std::make_unique<mock::MockMotorCommandEndpoint>(second_model_, EndpointId{2}, "second");
            first = a.get();
            second = b.get();
            (void)add_endpoint(std::move(a));
            (void)add_endpoint(std::move(b));
        }
        mock::MockMotorCommandEndpoint* first{nullptr};
        mock::MockMotorCommandEndpoint* second{nullptr};
        std::shared_ptr<mock::MotorModel> first_model_, second_model_;
    } twin;
    EdgeRig rig(false);
    KRITVA_CHECK(rig.registry.register_device(twin).has_value() && rig.hello().status.ok());
    for (const std::uint64_t e : {1, 2}) {
        KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, 6, e).status.ok() && rig.lifecycle(MessageType::START_REQUEST, 6, e).status.ok());
    }
    const std::uint64_t a = rig.send(MessageType::WRITE_REQUEST, write_payload(0.2, 6, 1));
    KRITVA_CHECK(rig.poll().size() == 1);
    const std::uint64_t b = rig.send(MessageType::WRITE_REQUEST, write_payload(0.4, 6, 2));
    KRITVA_CHECK(rig.poll().size() == 1 && twin.first_model_->velocity_rad_s == 0.2 && twin.second_model_->velocity_rad_s == 0.4);
    // The last applied write of the SECOND endpoint is b; the FIRST endpoint's last applied write is still a, and is still a duplicate.
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.2, 6, 1), a, rig.session));
    KRITVA_CHECK(rig.poll().size() == 1 && rig.stats().writes_resent == 1);
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.4, 6, 2), b, rig.session));
    KRITVA_CHECK(rig.poll().size() == 1 && rig.stats().writes_resent == 2 && rig.stats().writes_applied == 2);
    // A duplicate of one endpoint's write addressed to the other is not a duplicate there: it is stale.
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.2, 6, 2), a, rig.session));
    KRITVA_CHECK(rig.poll().empty() && twin.second_model_->velocity_rad_s == 0.4);
}

// 1: session.
static void test_step1_session_wrong_or_old_session_never_writes() {
    EdgeRig rig;
    running_rig(rig);
    const std::uint64_t old_session = rig.session;
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.5), 100, old_session + 1));
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.5), 101, 0));
    KRITVA_CHECK(rig.poll().empty() && rig.stats().wrong_session == 2 && rig.motor.model().velocity_rad_s == 0.0);
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
    KRITVA_CHECK(rig.hello().status.ok());                                  // the session is replaced (the actuator is stopped)
    rig.send_raw(EdgeRig::frame(MessageType::WRITE_REQUEST, write_payload(0.9), 1000, old_session));
    KRITVA_CHECK(rig.poll().empty() && rig.motor.model().velocity_rad_s != 0.9);   // the old session can never write again
    KRITVA_CHECK(rig.motor.command().effective_velocity() == 0.0 && rig.stats().writes_applied == 1);
}

// The write reaches hardware only through the I3 endpoint: its counters and last error show it.
static void test_the_write_goes_through_the_i3_endpoint() {
    EdgeRig rig;
    running_rig(rig);
    auto& cmd = rig.motor.command();
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());
    KRITVA_CHECK(cmd.statistics().sample_count.value() == 1 && cmd.statistics().error_count.value() == 0 && !cmd.last_error());
    KRITVA_CHECK(rig.write(kMotor, kCommand, 7.0).status.code == ErrorCode::INVALID_ARGUMENT);
    KRITVA_CHECK(cmd.statistics().error_count.value() == 1 && cmd.last_error() && cmd.last_error()->code == ErrorCode::INVALID_ARGUMENT);
    // Stopping the endpoint (as the I3 contract defines it) zeroes the effective velocity, and the Edge then refuses writes.
    KRITVA_CHECK(rig.lifecycle(MessageType::STOP_REQUEST, kMotor, kCommand).status.ok());
    KRITVA_CHECK(cmd.effective_velocity() == 0.0 && rig.write(kMotor, kCommand, 0.5).status.code == ErrorCode::NOT_READY);
}

int main() {
    test_step2_address_direction_and_capability();
    test_step5_state();
    test_step6_limits_are_validated_before_apply();
    test_non_finite_commands_never_reach_the_endpoint();
    test_step7_apply_once_and_answer();
    test_step3_duplicate_of_the_last_write_is_answered_not_reapplied();
    test_same_sequence_with_a_different_command_is_never_applied();
    test_step4_stale_and_reordered_writes();
    test_a_rejected_write_is_not_recorded();
    test_the_ledger_is_per_endpoint();
    test_each_actuator_endpoint_has_its_own_ledger();
    test_step1_session_wrong_or_old_session_never_writes();
    test_the_write_goes_through_the_i3_endpoint();
    std::printf("edge_write_test: PASS\n");
    return 0;
}
