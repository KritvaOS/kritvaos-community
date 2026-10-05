//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : transport.hpp
// Description : Abstract frame transport: opaque bounded byte frames, link state and explicit time.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Transport
// Layer       : Hardware Abstraction
//
// Requirements: TR-001; TR-002
// API         : kritva::hardware::transport::Transport
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <kritva/core/core.hpp>

namespace kritva::hardware::transport {

/// The largest frame a transport carries, in bytes. A transport-level bound of its own: the transport
/// does not depend on the protocol, and a test checks that it equals the protocol's MAX_FRAME_SIZE.
inline constexpr std::size_t kMaxTransportFrameSize = 65536;

enum class LinkState : std::uint8_t { DISCONNECTED, CONNECTED };

[[nodiscard]] constexpr const char* link_state_name(LinkState s) noexcept {
    return s == LinkState::CONNECTED ? "CONNECTED" : "DISCONNECTED";
}

/// One end of a link that carries opaque frames. A transport never looks inside a frame: it
/// does not decode the header, check sessions, sequences, correlation or addresses, retry, or
/// make any safety decision. Those belong to the protocol, session and Edge layers.
///
/// Time is the transport's monotonic nanosecond clock. It moves only when advance() is called
/// (never by itself), so a frame can become observable only after the clock has been advanced.
/// A frame is delivered at most once per send, unless the link is told to duplicate it.
/// Control plane only: operations may allocate; no real-time guarantee is made.
class Transport {
public:
    virtual ~Transport() = default;

    /// Brings the link up (idempotent). Frames sent earlier were discarded when it went down.
    virtual core::Result<void> connect() = 0;
    /// Takes the link down (idempotent). Frames in flight in both directions are discarded.
    virtual void disconnect() = 0;
    [[nodiscard]] virtual LinkState link_state() const noexcept = 0;

    /// Hands one frame (1..kMaxTransportFrameSize (65536) bytes) to the link. It does not mean the frame will arrive.
    /// INVALID_ARGUMENT: empty or oversized. RESOURCE_UNAVAILABLE: link down, or the in-flight
    /// queue is full. The bytes are copied.
    virtual core::Result<void> send(std::span<const std::uint8_t> frame) = 0;

    /// The next frame whose delivery time has been reached, or nothing. Never blocks.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> receive() = 0;

    [[nodiscard]] virtual std::uint64_t now_ns() const noexcept = 0;
    /// Moves the clock forward by `dt_ns` (0 is allowed). INVALID_ARGUMENT if it would overflow.
    virtual core::Result<void> advance(std::uint64_t dt_ns) = 0;
};

} // namespace kritva::hardware::transport
