//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device_registry.cpp
// Description : Device registry implementation.
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

#include <kritva/hardware/device_registry.hpp>

#include <string>

namespace kritva::hardware {

using core::ErrorCode;
using core::Result;

namespace {

core::Error error(ErrorCode code, std::string message) {
    return core::Error{code, core::ErrorSeverity::ERROR, {}, {}, std::move(message)};
}

} // namespace

Result<void> DeviceRegistry::register_device(Device& device) {
    if (closed_) return Result<void>::failure(error(ErrorCode::INVALID_STATE, "the device registry is closed"));
    for (const Device* existing : devices_) {
        if (existing->info().id() == device.info().id()) {
            return Result<void>::failure(error(ErrorCode::INVALID_ARGUMENT, "duplicate device id"));
        }
        if (existing->info().name() == device.info().name()) {
            return Result<void>::failure(error(ErrorCode::INVALID_ARGUMENT, "duplicate device name '" + device.info().name() + "'"));
        }
    }
    devices_.push_back(&device);
    return Result<void>::success();
}

Result<Device*> DeviceRegistry::find(DeviceId id) const {
    for (Device* d : devices_) if (d->info().id() == id) return Result<Device*>::success(d);
    return Result<Device*>::failure(error(ErrorCode::INVALID_ARGUMENT, "no device with id " + std::to_string(id.value())));
}

Result<Device*> DeviceRegistry::find(std::string_view device_name) const {
    for (Device* d : devices_) if (d->info().name() == device_name) return Result<Device*>::success(d);
    return Result<Device*>::failure(error(ErrorCode::INVALID_ARGUMENT, "no device named '" + std::string(device_name) + "'"));
}

Result<Endpoint*> DeviceRegistry::find_endpoint(DeviceId device, EndpointId endpoint) const {
    const auto d = find(device);
    if (!d) return Result<Endpoint*>::failure(d.error());
    if (Endpoint* e = d.value()->find_endpoint(endpoint)) return Result<Endpoint*>::success(e);
    return Result<Endpoint*>::failure(error(ErrorCode::INVALID_ARGUMENT,
        "device '" + d.value()->info().name() + "' has no endpoint with id " + std::to_string(endpoint.value())));
}

Result<Endpoint*> DeviceRegistry::find_endpoint(std::string_view device_name, std::string_view endpoint_name) const {
    const auto d = find(device_name);
    if (!d) return Result<Endpoint*>::failure(d.error());
    if (Endpoint* e = d.value()->find_endpoint(endpoint_name)) return Result<Endpoint*>::success(e);
    return Result<Endpoint*>::failure(error(ErrorCode::INVALID_ARGUMENT,
        "device '" + d.value()->info().name() + "' has no endpoint named '" + std::string(endpoint_name) + "'"));
}

} // namespace kritva::hardware
