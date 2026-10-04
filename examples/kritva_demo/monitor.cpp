//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : monitor.cpp
// Description : Demo Monitor implementation.
//
// Component   : KritvaOS Demo
// Module      : Reference Demo
// Layer       : Application
//
// Requirements: RR-OBS-001..007; RR-FLT-002
// API         : kritva::demo::Monitor
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include "monitor.hpp"

namespace kritva::demo {

void Monitor::tick() {
    if (!in(core::LifecycleState::RUNNING)) return;
    stats_.sample_count.increment();
    const auto report = host_.failure_report();
    if (!report) {
        stats_.error_count.increment();
        return;
    }
    failed_ = report.value().failed;
    affected_ = report.value().affected;
}

} // namespace kritva::demo
