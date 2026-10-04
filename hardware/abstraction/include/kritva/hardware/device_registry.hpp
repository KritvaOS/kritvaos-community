//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device_registry.hpp
// Description : Device registry: deterministic registration and discovery of Devices and their Endpoints.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Registry
// Layer       : Hardware Abstraction
//
// Requirements: DER-101..108; DER-206
// API         : kritva::hardware::DeviceRegistry
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <string_view>
#include <vector>

#include <kritva/hardware/device.hpp>

namespace kritva::hardware {

/// Discovery of Devices and Endpoints. It is distinct from the Core component
/// registry (DER-003): that one holds runtime Components, this one holds
/// physical or simulated Devices and, through them, their Endpoints.
///
/// OWNERSHIP: the registry does not own Devices (the integrator does, like Core
/// components); a registered Device must outlive the registry. Registration is
/// setup only and is closed by close() (the DeviceManager closes it at the
/// first initialize()); there is no unregistration.
///
/// DETERMINISM: enumeration is registration order. Lookups are exact; an unknown
/// device or endpoint fails with INVALID_ARGUMENT (Core has no NOT_FOUND code) and
/// a message naming what was missing. Errors never change the registry.
///
/// Thread-safety: none. Control plane only.
class DeviceRegistry {
public:
    /// Registers `device`. INVALID_ARGUMENT for a duplicate device id or name;
    /// INVALID_STATE once the registry is closed.
    core::Result<void> register_device(Device& device);

    /// Closes registration for good; idempotent.
    void close() noexcept { closed_ = true; }
    [[nodiscard]] bool closed() const noexcept { return closed_; }

    /// Registered Devices in registration order.
    [[nodiscard]] const std::vector<Device*>& devices() const noexcept { return devices_; }
    [[nodiscard]] std::size_t size() const noexcept { return devices_.size(); }

    [[nodiscard]] core::Result<Device*> find(DeviceId id) const;
    [[nodiscard]] core::Result<Device*> find(std::string_view device_name) const;

    /// An Endpoint is addressed by Device and Endpoint, by id or by name.
    [[nodiscard]] core::Result<Endpoint*> find_endpoint(DeviceId device, EndpointId endpoint) const;
    [[nodiscard]] core::Result<Endpoint*> find_endpoint(std::string_view device_name, std::string_view endpoint_name) const;

private:
    std::vector<Device*> devices_;
    bool closed_{false};
};

} // namespace kritva::hardware
