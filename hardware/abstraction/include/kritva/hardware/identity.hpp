//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : identity.hpp
// Description : Device and Endpoint identity: strong ids, validated names, info records.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Identity
// Layer       : Hardware Abstraction
//
// Requirements: DER-101; DER-102; DER-202; DER-302
// API         : kritva::hardware::DeviceInfo / EndpointInfo
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <algorithm>
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <kritva/core/core.hpp>

namespace kritva::hardware {

/// Strong identifier over Core's Id semantics: valid iff non-zero. DeviceId and
/// EndpointId are distinct types and cannot be mixed up.
template <class Tag>
class TaggedId {
public:
    constexpr TaggedId() noexcept = default;
    explicit constexpr TaggedId(std::uint64_t value) noexcept : id_(value) {}
    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return id_.value(); }
    [[nodiscard]] constexpr bool valid() const noexcept { return id_.valid(); }
    friend constexpr bool operator==(TaggedId, TaggedId) noexcept = default;
    friend constexpr auto operator<=>(TaggedId, TaggedId) noexcept = default;

private:
    core::Id id_{};
};

struct DeviceTag {};
struct EndpointTag {};
using DeviceId = TaggedId<DeviceTag>;
using EndpointId = TaggedId<EndpointTag>;

/// A name is 1..64 characters of [a-z0-9_]. The dot is excluded because it
/// separates the parts of a configuration key (`<device>.<endpoint>.<setting>`).
[[nodiscard]] inline bool valid_name(std::string_view name) noexcept {
    return !name.empty() && name.size() <= 64 && std::all_of(name.begin(), name.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}

/// Whether an endpoint produces data (sensor) or consumes commands (actuator).
enum class EndpointDirection : std::uint8_t { SENSOR, ACTUATOR };

namespace detail {
inline core::Error invalid_identity(const char* what) {
    return core::Error{core::ErrorCode::INVALID_ARGUMENT, core::ErrorSeverity::ERROR, {}, {}, what};
}
} // namespace detail

/// Immutable identity of a Device: a valid id and a valid unique name.
class DeviceInfo {
public:
    [[nodiscard]] static core::Result<DeviceInfo> create(DeviceId id, std::string name) {
        if (!id.valid()) return core::Result<DeviceInfo>::failure(detail::invalid_identity("device id must be non-zero"));
        if (!valid_name(name)) return core::Result<DeviceInfo>::failure(detail::invalid_identity("device name must be 1..64 characters of [a-z0-9_]"));
        return core::Result<DeviceInfo>::success(DeviceInfo(id, std::move(name)));
    }
    [[nodiscard]] DeviceId id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    DeviceInfo(DeviceId id, std::string name) : id_(id), name_(std::move(name)) {}
    DeviceId id_;
    std::string name_;
};

/// Immutable identity of an Endpoint within its Device.
class EndpointInfo {
public:
    [[nodiscard]] static core::Result<EndpointInfo> create(EndpointId id, std::string name, EndpointDirection direction) {
        if (!id.valid()) return core::Result<EndpointInfo>::failure(detail::invalid_identity("endpoint id must be non-zero"));
        if (!valid_name(name)) return core::Result<EndpointInfo>::failure(detail::invalid_identity("endpoint name must be 1..64 characters of [a-z0-9_]"));
        return core::Result<EndpointInfo>::success(EndpointInfo(id, std::move(name), direction));
    }
    [[nodiscard]] EndpointId id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] EndpointDirection direction() const noexcept { return direction_; }

private:
    EndpointInfo(EndpointId id, std::string name, EndpointDirection direction)
        : id_(id), name_(std::move(name)), direction_(direction) {}
    EndpointId id_;
    std::string name_;
    EndpointDirection direction_;
};

} // namespace kritva::hardware
