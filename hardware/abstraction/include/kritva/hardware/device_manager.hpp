//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device_manager.hpp
// Description : DeviceManager: the one Core Component that coordinates Devices and Endpoints for the runtime.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Device Manager
// Layer       : Hardware Abstraction
//
// Requirements: DER-004; DER-205; DER-401..407; DER-601..607; DER-704
// API         : kritva::hardware::DeviceManager
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <string>
#include <vector>

#include <kritva/core/core.hpp>
#include <kritva/hardware/device_registry.hpp>

namespace kritva::hardware {

/// The single point where Devices and Endpoints meet the runtime. It is ONE Core
/// `runtime::Component`, registered with the existing RuntimeHost::add_component();
/// RuntimeHost, RuntimeManager and Core are unchanged. Devices and Endpoints are
/// not Components (DER-003) and there is no second lifecycle engine (DER-407): the
/// manager runs the Core Component lifecycle table on a Core `Lifecycle`, and
/// fans each operation out to the endpoints.
///
/// ORDER (DER-405, DER-406). configure, initialize and start visit the enabled
/// devices in registration order and each device's endpoints in insertion order;
/// stop and shutdown visit exactly the reverse sequence.
///
/// CONFIGURATION (DER-205). `<device>.enabled` (bool, default true) switches a
/// whole device off: a disabled device is registered but never configured,
/// initialized, started or counted in health. Endpoint settings are
/// `<device>.<endpoint>.<setting>` for each name in Endpoint::setting_names(); the
/// endpoint receives only its own settings, under their bare names. config_keys()
/// lists every key, for the loader's allow-list so that typos are rejected.
///
/// FAILURE (DER-607, DER-704, DER-703).
///  - A failing endpoint during initialize/start/stop faults the endpoint, and the
///    manager itself enters FAULT and returns the endpoint's Error with
///    `source` = the manager's component id (Core Component rule).
///  - An endpoint that faults on its own while the manager is RUNNING (for example
///    a hardware failure on read) leaves the manager RUNNING: its health is
///    UNHEALTHY and its status FAILED (visible through the runtime's observation
///    and failure_report()), and exactly ONE ERROR event is reported to the event
///    sink, with the manager as source. The faulted endpoint is never restarted.
///  - stop() stops the healthy endpoints and skips faulted ones; shutdown()
///    releases everything, including faulted endpoints, so controlled shutdown
///    works after any failure. A restart is an explicit new lifecycle:
///    shutdown, then initialize again.
///
/// LIFETIMES. The manager owns neither Devices nor Endpoints; every registered
/// Device must outlive the manager (declare devices before it).
///
/// Thread-safety: none. Control plane only.
class DeviceManager final : public core::runtime::Component, public core::runtime::IComponentStatistics {
public:
    explicit DeviceManager(core::runtime::ComponentId id);
    ~DeviceManager() override;

    /// Registers a Device (setup only; see DeviceRegistry). Closed at initialize().
    core::Result<void> register_device(Device& device) { return registry_.register_device(device); }
    [[nodiscard]] const DeviceRegistry& registry() const noexcept { return registry_; }

    /// Where endpoint faults are reported (not owned). Optional.
    void set_event_sink(core::runtime::IEventSink* sink) noexcept { sink_ = sink; }

    /// All configuration keys of the registered devices, in registration order.
    [[nodiscard]] std::vector<std::string> config_keys() const;

    core::Result<void> configure(const core::Configuration& configuration) override;
    core::Result<void> initialize() override;
    core::Result<void> start() override;
    core::Result<void> stop() override;
    core::Result<void> shutdown() override;

    [[nodiscard]] core::LifecycleState lifecycle_state() const noexcept override { return lifecycle_.state(); }
    [[nodiscard]] core::Status status() const override;
    [[nodiscard]] core::Health health() const override;
    [[nodiscard]] core::CapabilitySet capabilities() const override;
    /// Sum of the enabled endpoints' operation statistics.
    [[nodiscard]] core::Statistics statistics() const override;

private:
    [[nodiscard]] bool enabled(const Device& device) const noexcept;
    template <class F> core::Result<void> each_endpoint(bool reverse, F op);
    core::Result<void> with_source(core::Result<void> result) const;
    core::Result<void> invalid_state(const char* operation) const;
    core::Result<void> fail(core::Result<void> failed);
    void install_listeners();
    void clear_listeners();
    void on_endpoint_fault(const Endpoint& endpoint);

    DeviceRegistry registry_;
    core::Lifecycle lifecycle_;
    std::vector<const Device*> disabled_;
    core::runtime::IEventSink* sink_{nullptr};
    std::string fault_detail_;
    bool live_{false};
    bool in_lifecycle_{false};
};

} // namespace kritva::hardware
