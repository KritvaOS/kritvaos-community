//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_endpoint.hpp
// Description : Nexus-side proxies of Edge endpoints. Each is an ordinary I3 typed Endpoint whose lifecycle
//               hooks and data operations are protocol requests.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: RR-001; RR-002; RR-003; SR-001 (the Edge stays authoritative)
// API         : kritva::hardware::remote::RemoteAccelerationEndpoint / RemoteAngularVelocityEndpoint /
//               RemotePositionEndpoint / RemoteMotorCommandEndpoint
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <kritva/hardware/remote/node.hpp>
#include <kritva/hardware/remote/remote_node.hpp>
#include <kritva/hardware/typed_endpoints.hpp>

namespace kritva::hardware::remote {

/// The part every remote endpoint shares: where it lives and how its lifecycle maps to requests. It is not an
/// Endpoint; the proxies below hold one.
class RemoteProxy {
public:
    RemoteProxy(RemoteNode& node, EndpointAddress address, std::vector<std::string> settings) noexcept
        : node_(node), address_(address), settings_(std::move(settings)) { node_.add_proxy(*this); }

    [[nodiscard]] const EndpointAddress& address() const noexcept { return address_; }
    [[nodiscard]] const std::vector<std::string>& settings() const noexcept { return settings_; }
    [[nodiscard]] RemoteNode& node() noexcept { return node_; }

    /// The endpoint binds its I3 state and its fault entry point, so that the node can apply the link-loss policy.
    void bind(std::function<core::LifecycleState()> state, std::function<void(const core::Error&)> fault) {
        state_ = std::move(state);
        fault_ = std::move(fault);
    }
    [[nodiscard]] core::LifecycleState state() const { return state_ ? state_() : core::LifecycleState::UNKNOWN; }
    /// Puts a LIVE endpoint into FAULT and records why (once per fault episode; an endpoint that is not live is left alone, and
    /// an origin is never overwritten while the endpoint stays FAULT). The I3 enter_fault notifies the fault listeners once.
    void fault(const core::Error& cause, FaultOrigin origin) {
        const core::LifecycleState s = state();
        if (!fault_ || (s != core::LifecycleState::READY && s != core::LifecycleState::RUNNING)) return;
        origin_ = origin;
        fault_(cause);
    }
    /// NONE unless the endpoint is FAULT; then what the Nexus recorded, or LOCAL_FAILURE if it faulted by its own I3 rules.
    [[nodiscard]] FaultOrigin origin() const {
        if (state() != core::LifecycleState::FAULT) return FaultOrigin::NONE;
        return origin_ == FaultOrigin::NONE ? FaultOrigin::LOCAL_FAILURE : origin_;
    }

    /// The Edge's own error (code and message, unchanged), the local TIMEOUT or the link error.
    core::Result<void> configure(const core::Configuration& scoped);
    core::Result<void> initialize();
    core::Result<void> start();
    core::Result<void> stop();
    core::Result<void> shutdown();

private:
    core::Result<void> lifecycle(transport::MessageType request);

    RemoteNode& node_;
    EndpointAddress address_;
    std::vector<std::string> settings_;
    std::function<core::LifecycleState()> state_;
    std::function<void(const core::Error&)> fault_;
    FaultOrigin origin_{FaultOrigin::NONE};
    bool holds_period_{false};
    bool cleanup_pending_{false};     ///< a shutdown happened while nobody was reachable: the Edge endpoint may still be live
};

/// Converts an Edge status into the error an I3 caller sees: the same code and message the Edge's endpoint gave.
[[nodiscard]] core::Error error_of(const transport::StatusField& status);

/// A sensor proxy. `Base` is the I3 typed endpoint (for example SensorEndpoint<AccelerationSample>); the
/// application-facing API is exactly that contract: nothing of the protocol appears in it.
template <class Base, class Sample>
class RemoteSensorEndpoint final : public Base {
public:
    RemoteSensorEndpoint(EndpointInfo info, core::CapabilitySet capabilities, RemoteNode& node, EndpointAddress address,
                         std::vector<std::string> settings)
        : Base(std::move(info), std::move(capabilities)), proxy_(node, address, std::move(settings)) {
        proxy_.bind([this] { return this->lifecycle_state(); }, [this](const core::Error& e) { (void)this->enter_fault(e); });
    }

    [[nodiscard]] std::vector<std::string> setting_names() const override { return proxy_.settings(); }

protected:
    core::Result<void> on_configure(const core::Configuration& scoped) override { return proxy_.configure(scoped); }
    core::Result<void> on_initialize() override { return proxy_.initialize(); }
    core::Result<void> on_start() override { return proxy_.start(); }
    core::Result<void> on_stop() override { return proxy_.stop(); }
    core::Result<void> on_shutdown() override { return proxy_.shutdown(); }
    core::Result<void> do_read(Sample& out) override;

private:
    RemoteProxy proxy_;
};

using RemoteAccelerationEndpoint = RemoteSensorEndpoint<AccelerationEndpoint, AccelerationSample>;
using RemoteAngularVelocityEndpoint = RemoteSensorEndpoint<AngularVelocityEndpoint, AngularVelocitySample>;
using RemotePositionEndpoint = RemoteSensorEndpoint<PositionEndpoint, PositionSample>;

/// The actuator proxy: an ActuatorEndpoint<MotorCommand>. The Edge stays authoritative for session, sequence,
/// state, limits and the application of every command: this class validates nothing but that the value can be
/// encoded (finite) and returns the Edge's own error unchanged.
class RemoteMotorCommandEndpoint final : public MotorCommandEndpoint {
public:
    RemoteMotorCommandEndpoint(EndpointInfo info, RemoteNode& node, EndpointAddress address, std::vector<std::string> settings)
        : MotorCommandEndpoint(std::move(info), motor_command_capability()), proxy_(node, address, std::move(settings)) {
        proxy_.bind([this] { return this->lifecycle_state(); }, [this](const core::Error& e) { (void)this->enter_fault(e); });
    }

    [[nodiscard]] std::vector<std::string> setting_names() const override { return proxy_.settings(); }

    /// Explicit retransmission of the LAST write request, with the same sequence and payload (never automatic).
    /// Use it after a write returned TIMEOUT, whose outcome is unknown: if the Edge applied the command, its
    /// session ledger answers from its cache without applying it again; if it never received it, it is applied
    /// now unless later traffic made it stale. Counted like a write; needs the endpoint RUNNING.
    core::Result<void> retransmit_last_write();

protected:
    core::Result<void> on_configure(const core::Configuration& scoped) override { return proxy_.configure(scoped); }
    core::Result<void> on_initialize() override { return proxy_.initialize(); }
    core::Result<void> on_start() override { return proxy_.start(); }
    core::Result<void> on_stop() override { return proxy_.stop(); }
    core::Result<void> on_shutdown() override { return proxy_.shutdown(); }
    core::Result<void> do_write(const MotorCommand& command) override;

private:
    RemoteProxy proxy_;
};

} // namespace kritva::hardware::remote
