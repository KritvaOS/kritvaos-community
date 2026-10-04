//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : controller.cpp
// Description : Demo Controller implementation.
//
// Component   : KritvaOS Demo
// Module      : Reference Demo
// Layer       : Application
//
// Requirements: RR-APP-002; RR-DEP-001
// API         : kritva::demo::Controller
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include "controller.hpp"

namespace kritva::demo {

core::Result<void> Controller::on_configure(const core::Configuration& cfg) {
    const auto gain = runtime::get_int(cfg, "controller.gain", 2, 1, 100);
    if (!gain) return core::Result<void>::failure(gain.error());
    gain_ = gain.value();
    return core::Result<void>::success();
}

bool Controller::tick() {
    if (!in(core::LifecycleState::RUNNING) || sensor_.lifecycle_state() != core::LifecycleState::RUNNING) return false;
    command_ = gain_ * sensor_.reading();
    stats_.sample_count.increment();
    return true;
}

} // namespace kritva::demo
