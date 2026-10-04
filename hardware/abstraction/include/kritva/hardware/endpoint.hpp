//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : endpoint.hpp
// Description : Endpoint contract: identity, lifecycle on Core Lifecycle, status, health, statistics, diagnostics.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Endpoint
// Layer       : Hardware Abstraction
//
// Requirements: DER-301..307; DER-401..404; DER-407; DER-604; DER-606; DER-607; DER-701; DER-703
// API         : kritva::hardware::Endpoint
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <kritva/core/core.hpp>
#include <kritva/hardware/identity.hpp>

namespace kritva::hardware {

/// One physical (or simulated) capability of a Device: an acceleration source, a
/// motor command sink, and so on. The base class carries no data operation;
/// SensorEndpoint<T> and ActuatorEndpoint<T> add the typed one.
///
/// LIFECYCLE (DER-401..404, DER-407). There is no state machine of its own: the
/// endpoint holds a released Core `Lifecycle` and follows the Core Component
/// operation table.
///
///   operation    valid from            on success   failing hook
///   configure    UNKNOWN, STOPPED      unchanged    state unchanged, error returned
///   initialize   UNKNOWN, STOPPED      READY        FAULT, error returned
///   start        READY                 RUNNING      FAULT, error returned
///   stop         READY, RUNNING        STOPPED      FAULT, error returned
///   shutdown     UNKNOWN, STOPPED (no-op, releases a live period once), FAULT -> STOPPED
///
/// An operation invalid for the current state fails with INVALID_STATE and
/// changes nothing. If a hook faults the endpoint itself (enter_fault()) the operation
/// fails: the hook's error is returned if it failed, else the fault's cause; the endpoint is
/// already FAULT and no second transition or notification happens. FAULT is left only by shutdown(): a faulted endpoint never
/// recovers by itself and no operation restarts it. initialize() may be called
/// again from STOPPED (a new live period).
///
/// ERRORS use released Core ErrorCodes only: invalid lifecycle call INVALID_STATE;
/// data operation while not RUNNING NOT_READY; on a FAULT endpoint
/// RESOURCE_UNAVAILABLE; injected hardware failure INTERNAL_ERROR; bad argument
/// INVALID_ARGUMENT. An endpoint returns Errors with an invalid `source`; the
/// DeviceManager that owns it sets its own component id as the source.
///
/// OBSERVATION: status(), health(), capabilities(), statistics() (successful and
/// failed data operations), last_error().
///
/// Thread-safety: none. Real-time: control plane only; operations may allocate
/// and block, and no real-time guarantee is made (DER-506).
class Endpoint {
public:
    virtual ~Endpoint() = default;
    Endpoint(const Endpoint&) = delete;
    Endpoint& operator=(const Endpoint&) = delete;

    [[nodiscard]] const EndpointInfo& info() const noexcept { return info_; }

    core::Result<void> configure(const core::Configuration& configuration);
    core::Result<void> initialize();
    core::Result<void> start();
    core::Result<void> stop();
    core::Result<void> shutdown();

    [[nodiscard]] core::LifecycleState lifecycle_state() const noexcept { return lifecycle_.state(); }

    /// FAILED in FAULT, UNKNOWN before the first initialize, OK otherwise.
    [[nodiscard]] core::Status status() const;
    /// UNHEALTHY (with detail) in FAULT; HEALTHY while RUNNING, or DEGRADED
    /// (with detail) if the implementation reported degradation; UNKNOWN otherwise.
    [[nodiscard]] core::Health health() const;
    [[nodiscard]] const core::CapabilitySet& capabilities() const noexcept { return capabilities_; }
    /// sample_count = successful data operations, error_count = failed ones.
    [[nodiscard]] const core::Statistics& statistics() const noexcept { return statistics_; }
    /// The configuration settings this endpoint reads, as bare names (for example
    /// `limit`). The DeviceManager looks up `<device>.<endpoint>.<setting>` for each
    /// and hands the endpoint only these, under their bare names, in configure().
    [[nodiscard]] virtual std::vector<std::string> setting_names() const { return {}; }

    /// Fault listeners: called once each time this endpoint enters FAULT, whether through
    /// a failing hook or through enter_fault() (never again while it stays faulted), after
    /// the on_fault() hook. The DeviceManager uses one to report a fault that happens on
    /// its own, outside a lifecycle call.
    ///
    /// Listeners are owner-scoped: `owner` is any stable address that identifies the
    /// registrant (typically `this`). Registering again with the same owner replaces that
    /// owner's listener; different owners coexist and are called in registration order;
    /// clear_fault_listener(owner) removes only that owner's listener. A registrant must
    /// clear its listener before it is destroyed.
    using FaultListener = std::function<void(const Endpoint&)>;
    void set_fault_listener(const void* owner, FaultListener listener);
    void clear_fault_listener(const void* owner) noexcept;

    /// The most recent failure of any operation; kept until replaced.
    [[nodiscard]] const std::optional<core::Error>& last_error() const noexcept { return last_error_; }

protected:
    Endpoint(EndpointInfo info, core::CapabilitySet capabilities)
        : info_(std::move(info)), capabilities_(std::move(capabilities)) {}

    /// Implementation hooks, called by the lifecycle operations above after the
    /// state check. The defaults succeed.
    virtual core::Result<void> on_configure(const core::Configuration&) { return core::Result<void>::success(); }
    virtual core::Result<void> on_initialize() { return core::Result<void>::success(); }
    virtual core::Result<void> on_start() { return core::Result<void>::success(); }
    virtual core::Result<void> on_stop() { return core::Result<void>::success(); }
    virtual core::Result<void> on_shutdown() { return core::Result<void>::success(); }

    /// Ok while RUNNING; NOT_READY before/after the live period; RESOURCE_UNAVAILABLE in FAULT.
    [[nodiscard]] core::Result<void> check_operational(const char* operation) const;

    /// Moves a READY or RUNNING endpoint to FAULT with `cause` as its last error and fault
    /// detail, calls on_fault(), then the fault listeners. Idempotent while faulted;
    /// INVALID_STATE in any other state (the endpoint is not yet live, is being initialized
    /// or stopped, or has been stopped). Never recovers by itself. A lifecycle hook may call
    /// it; the operation then fails (see the lifecycle table) without a second transition.
    core::Result<void> enter_fault(const core::Error& cause);

    /// Called exactly once on each transition into FAULT, after the state has changed and
    /// before the listeners. Implementations make their hardware safe here (an actuator
    /// zeroes its output). Must not throw and must not call back into lifecycle operations.
    virtual void on_fault() {}

    /// Reports (or clears) degradation of a RUNNING endpoint, observable as DEGRADED health.
    void set_degraded(std::string detail);
    void clear_degraded() { degraded_detail_.reset(); }

    /// Bookkeeping for typed data operations.
    void count_operation(bool succeeded) noexcept;
    void note_error(const core::Error& error) { last_error_ = error; }

    [[nodiscard]] static core::Error make_error(core::ErrorCode code, std::string message);

private:
    void notify_fault();
    core::Result<void> invalid_state(const char* operation);
    core::Result<void> fail_into_fault(core::Result<void> failed);
    core::Result<void> complete(core::Result<void> hook_result, core::LifecycleState on_success);
    void transition(core::LifecycleState target);

    EndpointInfo info_;
    core::CapabilitySet capabilities_;
    core::Lifecycle lifecycle_;
    core::Statistics statistics_;
    std::optional<core::Error> last_error_;
    std::optional<std::string> degraded_detail_;
    std::string fault_detail_;
    std::vector<std::pair<const void*, FaultListener>> fault_listeners_;
    bool live_{false};
};

} // namespace kritva::hardware
