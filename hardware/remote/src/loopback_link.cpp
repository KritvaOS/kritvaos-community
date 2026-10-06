//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : loopback_link.cpp
// Description : The explicit same-process driver.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Remote
// Layer       : Hardware Abstraction
//
// Requirements: RI-001; RR-002; TR-001
// API         : kritva::hardware::remote::LoopbackLink
//
// Author      : KritvaOS
// Created     : 06-10-2026
//==============================================================================

#include <kritva/hardware/remote/loopback_link.hpp>

#include <algorithm>

namespace kritva::hardware::remote {

void LoopbackLink::pump() {
    edge_.poll();
    if (nexus_ != nullptr) nexus_->service();
}

core::Result<void> LoopbackLink::advance(std::uint64_t duration_ns) {
    std::uint64_t remaining = duration_ns;
    while (remaining > 0) {
        const std::uint64_t step = std::min(quantum_ns_, remaining);
        if (auto moved = transport_.advance(step); !moved) return moved;
        remaining -= step;
        pump();
    }
    return core::Result<void>::success();
}

} // namespace kritva::hardware::remote
