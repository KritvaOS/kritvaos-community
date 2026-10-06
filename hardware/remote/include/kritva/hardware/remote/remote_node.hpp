//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_node.hpp
// Description : One Edge as seen from the Nexus: the session, the discovered RemoteDevices and the live period
//               that ties endpoint initialization to a fresh session.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: RR-001; RR-002; NDR-003; ER-002
// API         : kritva::hardware::remote::RemoteNode / RemoteDevice
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <kritva/hardware/device.hpp>
#include <kritva/hardware/remote/remote_session.hpp>

namespace kritva::hardware::remote {

class RemoteNode;

/// A Device whose endpoints are RemoteEndpoints. It is an ordinary I3 Device: it registers in the existing
/// DeviceManager like any other (there is no RemoteDeviceManager) and has no lifecycle of its own.
class RemoteDevice final : public Device {
public:
    RemoteDevice(DeviceInfo info, RemoteNode& node) : Device(std::move(info)), node_(node) {}
    [[nodiscard]] RemoteNode& node() noexcept { return node_; }

private:
    RemoteNode& node_;
};

/// Setting names the Nexus may send to a remote endpoint, by (device name, endpoint name). Protocol 1.0 does
/// not carry the Edge's setting names, so the integrator declares them; `DeviceManager::config_keys()` then
/// lists `<device>.<endpoint>.<setting>` for them and its configure scopes the values to the endpoint.
using RemoteSettings = std::map<std::pair<std::string, std::string>, std::vector<std::string>>;

class RemoteNode {
public:
    RemoteNode(transport::Transport& link, RemoteSessionConfig config, RemoteSettings settings = {});
    RemoteNode(const RemoteNode&) = delete;
    RemoteNode& operator=(const RemoteNode&) = delete;

    /// connect(): HELLO and discovery, then one RemoteDevice per discovered device with one typed RemoteEndpoint
    /// per discovered endpoint (the capability id selects the type, protocol section 13). Once.
    core::Result<void> connect();

    /// The devices connect() built; the integrator registers them with the DeviceManager. The node owns them.
    [[nodiscard]] const std::vector<std::unique_ptr<RemoteDevice>>& devices() const noexcept { return devices_; }

    [[nodiscard]] RemoteSession& session() noexcept { return session_; }
    [[nodiscard]] const RemoteSession& session() const noexcept { return session_; }

    /// Live period (called by the endpoints): the first endpoint to initialize after none was live establishes a
    /// FRESH session (HELLO, discovery, topology equivalence with the registered one); later ones share it.
    /// The period ends when every endpoint that started it has shut down.
    core::Result<void> acquire();
    void release() noexcept;
    [[nodiscard]] std::size_t live_endpoints() const noexcept { return live_; }

private:
    RemoteSession session_;
    RemoteSettings settings_;
    std::vector<std::unique_ptr<RemoteDevice>> devices_;
    std::size_t live_{0};
    bool connected_once_{false};
};

} // namespace kritva::hardware::remote
