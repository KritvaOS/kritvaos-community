//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : simulated_transport.cpp
// Description : Deterministic simulated link.
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

#include <kritva/hardware/transport/simulated_transport.hpp>

#include <algorithm>
#include <limits>

#include <kritva/hardware/transport/protocol.hpp>

namespace kritva::hardware::transport {

namespace {

using core::ErrorCode;

constexpr std::uint64_t kMaxU64 = std::numeric_limits<std::uint64_t>::max();
constexpr std::size_t kMaxQueueCapacity = 65536;

core::Error make_error(ErrorCode code, std::string message) {
    return core::Error{code, core::ErrorSeverity::ERROR, {}, {}, std::move(message)};
}

std::uint64_t saturating_add(std::uint64_t a, std::uint64_t b) noexcept {
    return a > kMaxU64 - b ? kMaxU64 : a + b;
}

// xorshift64*: a fixed, portable generator, so the same seed gives the same sequence everywhere.
std::uint64_t next_random(std::uint64_t& state) noexcept {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * 2685821657736338717ull;
}

bool valid(const DirectionConfig& c) noexcept {
    return c.queue_capacity >= 1 && c.queue_capacity <= kMaxQueueCapacity && c.drop_permille <= 1000 &&
           c.duplicate_permille <= 1000 && c.reorder_permille <= 1000;
}

} // namespace

SimulatedTransport::SimulatedTransport()
    : nexus_end_(*this, LinkDirection::NEXUS_TO_EDGE, LinkDirection::EDGE_TO_NEXUS),
      edge_end_(*this, LinkDirection::EDGE_TO_NEXUS, LinkDirection::NEXUS_TO_EDGE) {
    for (Lane& l : lanes_) l.rng = l.config.seed;
}

core::Result<void> SimulatedTransport::configure(LinkDirection direction, DirectionConfig config) {
    if (!valid(config)) {
        return core::Result<void>::failure(make_error(
            ErrorCode::INVALID_ARGUMENT, "invalid link configuration: queue_capacity 1..65536 and permille values 0..1000"));
    }
    Lane& l = lane(direction);
    l.rng = config.seed == 0 ? 1 : config.seed;          // xorshift must not start at zero; 0 and 1 are equivalent seeds
    l.config = std::move(config);
    l.send_index = 0;
    return core::Result<void>::success();
}

core::Result<void> SimulatedTransport::connect() {
    state_ = LinkState::CONNECTED;
    return core::Result<void>::success();
}

void SimulatedTransport::disconnect() {
    if (state_ == LinkState::DISCONNECTED) return;
    state_ = LinkState::DISCONNECTED;
    for (Lane& l : lanes_) {
        l.stats.discarded_on_disconnect += l.queue.size();
        l.queue.clear();
    }
}

core::Result<void> SimulatedTransport::advance(std::uint64_t dt_ns) {
    if (dt_ns > kMaxU64 - now_ns_) {
        return core::Result<void>::failure(make_error(ErrorCode::INVALID_ARGUMENT, "advance would overflow the virtual clock"));
    }
    now_ns_ += dt_ns;
    return core::Result<void>::success();
}

bool SimulatedTransport::step() {
    bool any = false;
    std::uint64_t earliest = kMaxU64;
    for (const Lane& l : lanes_) {
        if (l.queue.empty()) continue;
        any = true;
        earliest = std::min(earliest, l.queue.begin()->first.first);
    }
    if (!any) return false;
    if (earliest > now_ns_) now_ns_ = earliest;
    return true;
}

std::size_t SimulatedTransport::pending(LinkDirection direction) const noexcept { return lane(direction).queue.size(); }

const DirectionStats& SimulatedTransport::stats(LinkDirection direction) const noexcept { return lane(direction).stats; }

void SimulatedTransport::enqueue(Lane& l, std::uint64_t delay_ns, const std::vector<std::uint8_t>& bytes) {
    l.queue.emplace(std::make_pair(saturating_add(now_ns_, delay_ns), l.order++), bytes);
}

core::Result<void> SimulatedTransport::send(LinkDirection direction, std::span<const std::uint8_t> frame) {
    Lane& l = lane(direction);
    if (frame.empty() || frame.size() > kMaxFrameSize) {
        ++l.stats.rejected_invalid;
        return core::Result<void>::failure(make_error(ErrorCode::INVALID_ARGUMENT, "a frame must be 1..65536 bytes"));
    }
    if (state_ != LinkState::CONNECTED) {
        ++l.stats.rejected_link_down;
        return core::Result<void>::failure(make_error(ErrorCode::RESOURCE_UNAVAILABLE, "the link is down"));
    }
    if (l.queue.size() >= l.config.queue_capacity) {
        ++l.stats.rejected_queue_full;
        return core::Result<void>::failure(make_error(ErrorCode::RESOURCE_UNAVAILABLE, "the link queue is full"));
    }

    // Always three draws per accepted send, so one decision never shifts the others.
    const std::uint64_t r_drop = next_random(l.rng) % 1000;
    const std::uint64_t r_dup = next_random(l.rng) % 1000;
    const std::uint64_t r_reorder = next_random(l.rng) % 1000;
    FrameFault f;
    if (const auto it = l.config.faults.find(l.send_index); it != l.config.faults.end()) f = it->second;
    ++l.send_index;
    ++l.stats.sent;

    if (f.drop || r_drop < l.config.drop_permille) {
        ++l.stats.dropped;
        return core::Result<void>::success();
    }
    std::uint64_t delay = saturating_add(l.config.latency_ns, f.extra_delay_ns);
    if (r_reorder < l.config.reorder_permille) delay = saturating_add(delay, l.config.reorder_delay_ns);

    const std::vector<std::uint8_t> bytes(frame.begin(), frame.end());
    enqueue(l, delay, bytes);
    // A duplicate copy is made only if the queue still has room, so the bound is never exceeded.
    if ((f.duplicate || r_dup < l.config.duplicate_permille) && l.queue.size() < l.config.queue_capacity) {
        enqueue(l, saturating_add(l.config.latency_ns, f.duplicate_extra_delay_ns), bytes);
        ++l.stats.duplicated;
    }
    return core::Result<void>::success();
}

std::optional<std::vector<std::uint8_t>> SimulatedTransport::receive(LinkDirection direction) {
    Lane& l = lane(direction);
    if (l.queue.empty() || l.queue.begin()->first.first > now_ns_) return std::nullopt;
    std::vector<std::uint8_t> bytes = std::move(l.queue.begin()->second);
    l.queue.erase(l.queue.begin());
    ++l.stats.delivered;
    return bytes;
}

} // namespace kritva::hardware::transport
