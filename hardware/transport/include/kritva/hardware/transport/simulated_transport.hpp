//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : simulated_transport.hpp
// Description : Deterministic in-process simulated link with injectable latency, drop, reorder,
//               duplicate delivery and disconnect, driven by an explicit virtual clock.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Transport
// Layer       : Hardware Abstraction
//
// Requirements: TR-001; TR-002
// API         : kritva::hardware::transport::SimulatedTransport
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include <kritva/hardware/transport/transport.hpp>

namespace kritva::hardware::transport {

enum class LinkDirection : std::uint8_t { NEXUS_TO_EDGE, EDGE_TO_NEXUS };

/// Fault for one particular frame of a direction, selected by the 0-based index of the accepted
/// send of that direction (frames refused by send() consume no index).
struct FrameFault {
    bool drop{false};
    std::uint64_t extra_delay_ns{0};               ///< added to the latency; a larger delay on an earlier frame reorders it
    bool duplicate{false};                         ///< a second copy is delivered (not made if the queue is then full)
    std::uint64_t duplicate_extra_delay_ns{0};     ///< added to the latency of the copy
};

/// Behavior of one direction. Probabilities are in permille (0..1000) and are drawn from a
/// seeded generator, three draws per accepted send in a fixed order (drop, duplicate, reorder),
/// so one configuration always yields the same behavior. Explicit `faults` combine with them.
struct DirectionConfig {
    std::uint64_t latency_ns{0};
    std::size_t queue_capacity{1024};              ///< frames in flight per direction, 1..65536
    std::uint64_t seed{1};
    std::uint16_t drop_permille{0};
    std::uint16_t duplicate_permille{0};
    std::uint16_t reorder_permille{0};             ///< a reordered frame gets `reorder_delay_ns` extra delay
    std::uint64_t reorder_delay_ns{0};
    std::map<std::uint64_t, FrameFault> faults;    ///< by send index
};

struct DirectionStats {
    std::uint64_t sent{0};                         ///< accepted by send()
    std::uint64_t delivered{0};                    ///< returned by receive()
    std::uint64_t dropped{0};                      ///< lost by fault injection
    std::uint64_t duplicated{0};                   ///< extra copies created
    std::uint64_t discarded_on_disconnect{0};      ///< in flight when the link went down
    std::uint64_t rejected_link_down{0};
    std::uint64_t rejected_queue_full{0};
    std::uint64_t rejected_invalid{0};
};

/// Two Transport ends (nexus() and edge()) joined by a simulated link with one shared virtual
/// clock. No threads, no sleeping, no wall clock, no sockets, no hidden randomness. Frames are
/// stored and returned byte for byte; the link never reads them.
///
/// Delivery: a frame accepted at time t is due at t + latency + any extra delay, and receive()
/// returns the frames whose due time is <= now_ns(), in (due time, send order) order. With zero
/// latency a frame is therefore receivable at once; with latency L it is not before t + L and is
/// at t + L exactly. A duplicate copy follows its original in send order.
///
/// The link starts DISCONNECTED. connect() brings it up; disconnect() takes it down and
/// discards everything in flight in both directions (counted). Neither implements any safety
/// behavior: link loss is only a connectivity fact that a later layer reacts to.
class SimulatedTransport {
public:
    SimulatedTransport();
    SimulatedTransport(const SimulatedTransport&) = delete;
    SimulatedTransport& operator=(const SimulatedTransport&) = delete;

    [[nodiscard]] Transport& nexus() noexcept { return nexus_end_; }
    [[nodiscard]] Transport& edge() noexcept { return edge_end_; }

    /// Replaces the behavior of one direction and restarts its send index and generator.
    /// Frames already in flight keep their due times. INVALID_ARGUMENT for out-of-range values.
    core::Result<void> configure(LinkDirection direction, DirectionConfig config);

    core::Result<void> connect();
    void disconnect();
    [[nodiscard]] LinkState link_state() const noexcept { return state_; }

    [[nodiscard]] std::uint64_t now_ns() const noexcept { return now_ns_; }
    core::Result<void> advance(std::uint64_t dt_ns);
    /// Jumps the clock to the earliest due time of any frame in flight, if that is in the future.
    /// Returns true iff a frame is receivable afterwards. With nothing in flight the clock
    /// does not move and the result is false.
    bool step();

    [[nodiscard]] std::size_t pending(LinkDirection direction) const noexcept;
    [[nodiscard]] const DirectionStats& stats(LinkDirection direction) const noexcept;

private:
    struct Lane {
        DirectionConfig config;
        DirectionStats stats;
        std::uint64_t send_index{0};
        std::uint64_t rng{1};
        std::uint64_t order{0};
        std::map<std::pair<std::uint64_t, std::uint64_t>, std::vector<std::uint8_t>> queue;   // (due, order)
    };

    class End final : public Transport {
    public:
        End(SimulatedTransport& owner, LinkDirection out, LinkDirection in) noexcept : owner_(owner), out_(out), in_(in) {}
        core::Result<void> connect() override { return owner_.connect(); }
        void disconnect() override { owner_.disconnect(); }
        [[nodiscard]] LinkState link_state() const noexcept override { return owner_.link_state(); }
        core::Result<void> send(std::span<const std::uint8_t> frame) override { return owner_.send(out_, frame); }
        [[nodiscard]] std::optional<std::vector<std::uint8_t>> receive() override { return owner_.receive(in_); }
        [[nodiscard]] std::uint64_t now_ns() const noexcept override { return owner_.now_ns(); }
        core::Result<void> advance(std::uint64_t dt_ns) override { return owner_.advance(dt_ns); }
    private:
        SimulatedTransport& owner_;
        LinkDirection out_;
        LinkDirection in_;
    };

    core::Result<void> send(LinkDirection direction, std::span<const std::uint8_t> frame);
    std::optional<std::vector<std::uint8_t>> receive(LinkDirection direction);
    [[nodiscard]] Lane& lane(LinkDirection d) noexcept { return lanes_[static_cast<std::size_t>(d)]; }
    [[nodiscard]] const Lane& lane(LinkDirection d) const noexcept { return lanes_[static_cast<std::size_t>(d)]; }
    void enqueue(Lane& l, std::uint64_t delay_ns, const std::vector<std::uint8_t>& bytes);

    LinkState state_{LinkState::DISCONNECTED};
    std::uint64_t now_ns_{0};
    Lane lanes_[2];
    End nexus_end_;
    End edge_end_;
};

} // namespace kritva::hardware::transport
