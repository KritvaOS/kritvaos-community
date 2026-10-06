//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : edge_service_test.cpp
// Description : Unit tests of the EdgeHost service: address validation, configure, lifecycle, read and observe
//               over mock devices, each returning the I3 endpoint's own result.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: ER-001; ER-003; PR-002
// API         : EDGE-SERVICE-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <cmath>
#include <string>

#include "../support/edge_rig.hpp"

using namespace kritva::hardware::remote;
using namespace kritva::hardware::remote::test;
using kritva::core::ErrorCode;
using kritva::core::HealthState;
using kritva::core::LifecycleState;

static Bytes configure_payload(std::uint64_t device, std::uint64_t endpoint, std::vector<ConfigureSetting> settings, std::uint64_t node = kEdgeNode) {
    return must(encode(ConfigureRequestPayload{AddressPayload{node, device, endpoint}, std::move(settings)}));
}

static void test_every_request_validates_the_address() {                // ER-003
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    struct Bad { const char* what; std::uint64_t node, device, endpoint; const char* message; };
    const Bad bads[] = {
        {"wrong node", kEdgeNode + 1, kImu, kAccel, "unknown node"},
        {"unknown device", kEdgeNode, 99, kAccel, "unknown device"},
        {"unknown endpoint", kEdgeNode, kImu, 99, "unknown endpoint"},
        {"endpoint of another device", kEdgeNode, kImu, kPosition + 7, "unknown endpoint"},
    };
    const MessageType requests[] = {MessageType::INITIALIZE_REQUEST, MessageType::START_REQUEST, MessageType::STOP_REQUEST,
                                    MessageType::SHUTDOWN_REQUEST, MessageType::CONFIGURE_REQUEST, MessageType::READ_REQUEST,
                                    MessageType::OBSERVE_REQUEST, MessageType::WRITE_REQUEST};
    for (const Bad& b : bads) {
        for (const MessageType type : requests) {
            Bytes payload;
            if (type == MessageType::CONFIGURE_REQUEST) payload = configure_payload(b.device, b.endpoint, {}, b.node);
            else if (type == MessageType::WRITE_REQUEST) payload = must(encode(WriteRequestPayload{AddressPayload{b.node, b.device, b.endpoint}, 0.1}));
            else payload = addr(b.device, b.endpoint, b.node);
            const Reply r = rig.call(type, payload);
            StatusField status;
            switch (type) {
                case MessageType::READ_REQUEST: status = decode_read_response(r.payload, ReadKind::VEC3).value.status; break;
                case MessageType::OBSERVE_REQUEST: status = decode<ObserveResponsePayload>(r.payload).value.status; break;
                case MessageType::WRITE_REQUEST: status = decode<StatusPayload>(r.payload).value.status; break;
                default: status = decode<LifecycleResponsePayload>(r.payload).value.status; break;
            }
            if (status.code != ErrorCode::INVALID_ARGUMENT || status.message != b.message) {
                std::fprintf(stderr, "%s on %s: code %d message '%s'\n", b.what, message_type_name(type), static_cast<int>(status.code), status.message.c_str());
                std::exit(1);
            }
        }
    }
    // Nothing was touched.
    for (const auto& [d, e] : EdgeRig::all_endpoints()) {
        const auto* ep = d == kImu ? (e == kAccel ? static_cast<const kritva::hardware::Endpoint*>(&rig.imu.acceleration()) : &rig.imu.angular_velocity())
                                   : (e == kCommand ? static_cast<const kritva::hardware::Endpoint*>(&rig.motor.command()) : &rig.motor.position());
        KRITVA_CHECK(ep->lifecycle_state() == LifecycleState::UNKNOWN);
    }
}

static void test_lifecycle_runs_on_the_i3_endpoint() {                   // ER-001, PR-002
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    auto& cmd = rig.motor.command();
    auto r = rig.lifecycle(MessageType::INITIALIZE_REQUEST, kMotor, kCommand);
    KRITVA_CHECK(r.status.ok() && r.state == LifecycleState::READY && cmd.lifecycle_state() == LifecycleState::READY);
    r = rig.lifecycle(MessageType::START_REQUEST, kMotor, kCommand);
    KRITVA_CHECK(r.status.ok() && r.state == LifecycleState::RUNNING);
    r = rig.lifecycle(MessageType::STOP_REQUEST, kMotor, kCommand);
    KRITVA_CHECK(r.status.ok() && r.state == LifecycleState::STOPPED);
    r = rig.lifecycle(MessageType::SHUTDOWN_REQUEST, kMotor, kCommand);
    KRITVA_CHECK(r.status.ok() && cmd.lifecycle_state() == LifecycleState::STOPPED);
    // Another endpoint is untouched by it.
    KRITVA_CHECK(rig.motor.position().lifecycle_state() == LifecycleState::UNKNOWN);
}

static void test_invalid_transitions_return_the_endpoints_own_error() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    auto& cmd = rig.motor.command();
    const auto direct_start = cmd.start();                                    // what the I3 endpoint itself says (state UNKNOWN)
    KRITVA_CHECK(!direct_start.has_value());
    const auto r = rig.lifecycle(MessageType::START_REQUEST, kMotor, kCommand);
    KRITVA_CHECK(!r.status.ok() && r.status.code == direct_start.error().code && r.status.message == direct_start.error().message);
    KRITVA_CHECK(cmd.lifecycle_state() == LifecycleState::UNKNOWN);          // refused calls change nothing
    KRITVA_CHECK(rig.lifecycle(MessageType::STOP_REQUEST, kMotor, kCommand).status.code == ErrorCode::INVALID_STATE);
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kMotor, kCommand).status.ok());
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kMotor, kCommand).status.code == ErrorCode::INVALID_STATE);   // twice
}

static void test_a_failing_endpoint_faults_and_reports_unchanged() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    auto& cmd = rig.motor.command();
    // fault_after_writes=1: the endpoint faults itself after its first successful write (I3 mock injection).
    const auto c = rig.call(MessageType::CONFIGURE_REQUEST, configure_payload(kMotor, kCommand, {{"fault_after_writes", std::int64_t{1}}}));
    KRITVA_CHECK(decode<LifecycleResponsePayload>(c.payload).value.status.ok());
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kMotor, kCommand).status.ok() && rig.lifecycle(MessageType::START_REQUEST, kMotor, kCommand).status.ok());
    KRITVA_CHECK(rig.write(kMotor, kCommand, 0.5).status.ok());              // this write succeeds, then the endpoint faults
    KRITVA_CHECK(cmd.lifecycle_state() == LifecycleState::FAULT);
    const auto again = rig.write(kMotor, kCommand, 0.1);
    KRITVA_CHECK(again.status.code == ErrorCode::RESOURCE_UNAVAILABLE);       // a faulted endpoint: the I3 error, unchanged
    const auto o = rig.observe(kMotor, kCommand);
    KRITVA_CHECK(o.status.ok() && o.state == LifecycleState::FAULT && o.health == HealthState::UNHEALTHY && !o.health_detail.empty());
    KRITVA_CHECK(o.has_last_error && o.last_error_code == ErrorCode::RESOURCE_UNAVAILABLE);
    KRITVA_CHECK(rig.lifecycle(MessageType::START_REQUEST, kMotor, kCommand).status.code == ErrorCode::INVALID_STATE);        // FAULT is left only by shutdown
    KRITVA_CHECK(rig.lifecycle(MessageType::SHUTDOWN_REQUEST, kMotor, kCommand).status.ok() && cmd.lifecycle_state() == LifecycleState::STOPPED);
}

static void test_configure_validates_before_dispatch() {                 // ER-003
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    auto& cmd = rig.motor.command();
    const auto status_of = [&](const Bytes& payload) { return decode<LifecycleResponsePayload>(rig.call(MessageType::CONFIGURE_REQUEST, payload).payload).value; };
    // Valid: known keys, scoped under their bare names, applied by Endpoint::configure.
    auto r = status_of(configure_payload(kMotor, kCommand, {{"min_rad_s", std::int64_t{-5}}, {"max_rad_s", std::int64_t{5}}}));
    KRITVA_CHECK(r.status.ok() && cmd.limits().min_rad_s == -5.0 && cmd.limits().max_rad_s == 5.0);
    KRITVA_CHECK(status_of(configure_payload(kMotor, kCommand, {})).status.ok());                      // an empty list is valid and keeps the limits
    KRITVA_CHECK(cmd.limits().max_rad_s == 5.0);
    // Unknown key, a key of another endpoint (a dotted key cannot even be encoded: names are [a-z0-9_]), a duplicate: rejected by the Edge before dispatch.
    for (const auto& settings : std::vector<std::vector<ConfigureSetting>>{
             {{"nonsense", std::int64_t{1}}}, {{"initial", std::int64_t{1}}}}) {
        r = status_of(configure_payload(kMotor, kCommand, settings));
        KRITVA_CHECK(!r.status.ok() && r.status.code == ErrorCode::INVALID_ARGUMENT && r.status.message == "unknown setting");
    }
    r = status_of(configure_payload(kMotor, kCommand, {{"min_rad_s", std::int64_t{-1}}, {"min_rad_s", std::int64_t{-2}}}));
    KRITVA_CHECK(r.status.code == ErrorCode::INVALID_ARGUMENT && r.status.message == "duplicate setting" && cmd.limits().min_rad_s == -5.0);
    // The endpoint's own value checks answer for wrong types and ranges, unchanged.
    r = status_of(configure_payload(kMotor, kCommand, {{"min_rad_s", std::string("fast")}}));
    KRITVA_CHECK(!r.status.ok() && cmd.limits().min_rad_s == -5.0);
    r = status_of(configure_payload(kMotor, kCommand, {{"max_rad_s", std::int64_t{999999}}}));
    KRITVA_CHECK(!r.status.ok() && cmd.limits().max_rad_s == 5.0);
    r = status_of(configure_payload(kMotor, kCommand, {{"min_rad_s", std::int64_t{3}}, {"max_rad_s", std::int64_t{2}}}));
    KRITVA_CHECK(!r.status.ok() && cmd.limits().min_rad_s == -5.0 && cmd.limits().max_rad_s == 5.0);   // min > max rejected, nothing changed
    // Configure is refused in the wrong state by the endpoint.
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kMotor, kCommand).status.ok());
    KRITVA_CHECK(rig.lifecycle(MessageType::START_REQUEST, kMotor, kCommand).status.ok());
    r = status_of(configure_payload(kMotor, kCommand, {{"max_rad_s", std::int64_t{1}}}));
    KRITVA_CHECK(r.status.code == ErrorCode::INVALID_STATE && cmd.limits().max_rad_s == 5.0);
}

static void test_reads_are_typed_by_capability() {                       // PR-002
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    // Before the endpoint runs, the I3 state check answers.
    KRITVA_CHECK(rig.read(kImu, kAccel, ReadKind::VEC3).status.code == ErrorCode::NOT_READY);
    rig.bring_up_all();
    const auto a1 = rig.read(kImu, kAccel, ReadKind::VEC3);
    const auto a2 = rig.read(kImu, kAccel, ReadKind::VEC3);
    KRITVA_CHECK(a1.status.ok() && a1.kind == ReadKind::VEC3 && a1.sample_sequence == 1 && a2.sample_sequence == 2 && a2.timestamp_ns > a1.timestamp_ns);
    // The wire value is what the endpoint itself produces (the next sample is number 3).
    kritva::hardware::AccelerationSample direct;
    KRITVA_CHECK(rig.imu.acceleration().read(direct).has_value() && direct.sequence == 3);
    const auto a4 = rig.read(kImu, kAccel, ReadKind::VEC3);
    KRITVA_CHECK(a4.sample_sequence == 4 && std::isfinite(a4.x) && std::isfinite(a4.y) && std::isfinite(a4.z));
    const auto g = rig.read(kImu, kGyro, ReadKind::VEC3);
    KRITVA_CHECK(g.status.ok() && g.sample_sequence == 1);
    const auto p = rig.read(kMotor, kPosition, ReadKind::SCALAR);
    KRITVA_CHECK(p.status.ok() && p.kind == ReadKind::SCALAR && p.sample_sequence == 1);
    // A read of an actuator is rejected by the Edge, not forwarded.
    const auto bad = rig.read(kMotor, kCommand, ReadKind::VEC3);
    KRITVA_CHECK(bad.status.code == ErrorCode::INVALID_ARGUMENT && rig.motor.command().statistics().sample_count.value() == 0);
}

static void test_a_failing_read_reports_the_endpoint_error() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    rig.bring_up_all();
    rig.imu.acceleration().fail_next_operation();
    const auto r = rig.read(kImu, kAccel, ReadKind::VEC3);
    KRITVA_CHECK(!r.status.ok() && r.status.code == ErrorCode::INTERNAL_ERROR && r.status.message == "injected read failure");
    KRITVA_CHECK(rig.read(kImu, kAccel, ReadKind::VEC3).status.ok());           // a failed read does not fault the endpoint
}

static void test_observe_reports_status_health_statistics_and_last_error() {   // PR-002
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    auto o = rig.observe(kImu, kAccel);
    KRITVA_CHECK(o.status.ok() && o.state == LifecycleState::UNKNOWN && o.operations_ok == 0 && o.operations_failed == 0 && !o.has_last_error);
    rig.bring_up_all();
    for (int i = 0; i < 3; ++i) KRITVA_CHECK(rig.read(kImu, kAccel, ReadKind::VEC3).status.ok());
    rig.imu.acceleration().fail_next_operation();
    KRITVA_CHECK(!rig.read(kImu, kAccel, ReadKind::VEC3).status.ok());
    o = rig.observe(kImu, kAccel);
    KRITVA_CHECK(o.state == LifecycleState::RUNNING && o.status_code == kritva::core::StatusCode::OK && o.health == HealthState::HEALTHY);
    KRITVA_CHECK(o.operations_ok == 3 && o.operations_failed == 1 && o.has_last_error && o.last_error_code == ErrorCode::INTERNAL_ERROR);
    KRITVA_CHECK(o.last_error_message == "injected read failure");
    // The report matches the endpoint's own statistics.
    KRITVA_CHECK(o.operations_ok == rig.imu.acceleration().statistics().sample_count.value() && o.operations_failed == rig.imu.acceleration().statistics().error_count.value());
    // Degradation is reported as health with its reason; text from a device is sanitized to wire rules.
    rig.imu.acceleration().degrade(std::string("hot\n\"x\"\x01") + "\xC3\xA9 and a very long reason " + std::string(400, 'z'));
    o = rig.observe(kImu, kAccel);
    KRITVA_CHECK(o.status.ok() && o.health == HealthState::DEGRADED && o.health_detail.size() == 256);
    for (const char c : o.health_detail) KRITVA_CHECK(static_cast<unsigned char>(c) >= 0x20 && static_cast<unsigned char>(c) <= 0x7E);
    KRITVA_CHECK(o.health_detail.rfind("hot?", 0) == 0);
}

// TIMEOUT is local to a requester (protocol section 8) and is never sent: a hardware timeout goes out as INTERNAL_ERROR.
static void test_a_hardware_timeout_is_never_sent_as_timeout() {
    using namespace kritva::hardware;
    class SlowAcceleration final : public AccelerationEndpoint {
    public:
        SlowAcceleration() : AccelerationEndpoint(EndpointInfo::create(EndpointId{1}, "acceleration", EndpointDirection::SENSOR).value(), acceleration_capability()) {}
    protected:
        kritva::core::Result<void> do_read(AccelerationSample&) override {
            return kritva::core::Result<void>::failure(kritva::core::Error{ErrorCode::TIMEOUT, kritva::core::ErrorSeverity::ERROR, {}, {}, "sensor timed out"});
        }
    };
    EdgeRig rig(false);
    Device slow(DeviceInfo::create(DeviceId{4}, "slow").value());
    KRITVA_CHECK(slow.add_endpoint(std::make_unique<SlowAcceleration>()).has_value() && rig.registry.register_device(slow).has_value());
    KRITVA_CHECK(rig.hello().status.ok());
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, 4, 1).status.ok() && rig.lifecycle(MessageType::START_REQUEST, 4, 1).status.ok());
    const auto r = rig.read(4, 1, ReadKind::VEC3);
    KRITVA_CHECK(r.status.code == ErrorCode::INTERNAL_ERROR && r.status.message == "sensor timed out");
    const auto o = rig.observe(4, 1);
    KRITVA_CHECK(o.has_last_error && o.last_error_code == ErrorCode::INTERNAL_ERROR && o.last_error_message == "sensor timed out");
}

static void test_requests_do_not_change_other_endpoints() {
    EdgeRig rig;
    KRITVA_CHECK(rig.hello().status.ok());
    KRITVA_CHECK(rig.lifecycle(MessageType::INITIALIZE_REQUEST, kImu, kGyro).status.ok());
    KRITVA_CHECK(rig.imu.angular_velocity().lifecycle_state() == LifecycleState::READY && rig.imu.acceleration().lifecycle_state() == LifecycleState::UNKNOWN &&
                 rig.motor.command().lifecycle_state() == LifecycleState::UNKNOWN);
}

int main() {
    test_every_request_validates_the_address();
    test_lifecycle_runs_on_the_i3_endpoint();
    test_invalid_transitions_return_the_endpoints_own_error();
    test_a_failing_endpoint_faults_and_reports_unchanged();
    test_configure_validates_before_dispatch();
    test_reads_are_typed_by_capability();
    test_a_failing_read_reports_the_endpoint_error();
    test_observe_reports_status_health_statistics_and_last_error();
    test_a_hardware_timeout_is_never_sent_as_timeout();
    test_requests_do_not_change_other_endpoints();
    std::printf("edge_service_test: PASS\n");
    return 0;
}
