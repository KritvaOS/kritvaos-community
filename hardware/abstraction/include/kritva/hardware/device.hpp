//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device.hpp
// Description : Device contract: identity, owned endpoints, aggregated status, health and capabilities.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Device
// Layer       : Hardware Abstraction
//
// Requirements: DER-201..206; DER-601; DER-603; DER-605; DER-105; DER-106
// API         : kritva::hardware::Device
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include <kritva/hardware/endpoint.hpp>

namespace kritva::hardware {

/// A physical or simulated physical unit that exposes a set of Endpoints. A Device
/// is an identity plus the Endpoints it OWNS; it has no lifecycle of its own (the
/// lifecycle belongs to its Endpoints, DER-407) and is not a Core Component
/// (DER-003). Its status, health and capabilities are aggregated from the
/// endpoints on every call, never stored.
///
/// OWNERSHIP (DER-206): the integrator owns the Device; the Device owns its
/// Endpoints for its whole lifetime, so Endpoint pointers returned from it are
/// valid until the Device is destroyed. Endpoints are held in insertion order,
/// which is the deterministic enumeration order (DER-105, DER-106).
///
/// Thread-safety: none. Control plane only.
class Device {
public:
    explicit Device(DeviceInfo info) : info_(std::move(info)) {}
    virtual ~Device() = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    [[nodiscard]] const DeviceInfo& info() const noexcept { return info_; }

    /// Takes ownership of `endpoint` and returns it. Setup only. Fails with
    /// INVALID_ARGUMENT for a null endpoint or a duplicate endpoint id or name
    /// within this Device, and with INVALID_STATE once the Device is sealed or any
    /// endpoint of the Device has left UNKNOWN (the endpoint set is fixed after first use).
    core::Result<Endpoint*> add_endpoint(std::unique_ptr<Endpoint> endpoint);

    /// Fixes the endpoint set for good: add_endpoint() then fails with INVALID_STATE.
    /// Idempotent. The DeviceManager seals every registered Device when it closes the
    /// registry, so no endpoint can appear after the runtime has taken over.
    void seal() noexcept { sealed_ = true; }
    [[nodiscard]] bool sealed() const noexcept { return sealed_; }

    /// Endpoints in insertion order.
    [[nodiscard]] const std::vector<Endpoint*>& endpoints() const noexcept { return view_; }
    /// nullptr if absent.
    [[nodiscard]] Endpoint* find_endpoint(EndpointId id) const noexcept;
    [[nodiscard]] Endpoint* find_endpoint(std::string_view name) const noexcept;

    /// FAILED if any endpoint is FAILED; OK if there is at least one endpoint and
    /// all are OK; UNKNOWN otherwise.
    [[nodiscard]] core::Status status() const;
    /// UNHEALTHY if any endpoint is, else DEGRADED if any is, else HEALTHY if there
    /// is at least one endpoint and all are, else UNKNOWN. The detail names the
    /// first endpoint that decided the result.
    [[nodiscard]] core::Health health() const;
    /// The union of the endpoints' capabilities.
    [[nodiscard]] core::CapabilitySet capabilities() const;

private:
    DeviceInfo info_;
    std::vector<std::unique_ptr<Endpoint>> owned_;
    std::vector<Endpoint*> view_;
    bool sealed_{false};
};

} // namespace kritva::hardware
