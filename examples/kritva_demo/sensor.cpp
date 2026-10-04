//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : sensor.cpp
// Description : Demo Sensor implementation.
//
// Component   : KritvaOS Demo
// Module      : Reference Demo
// Layer       : Application
//
// Requirements: RR-APP-006; RR-FLT-001..004
// API         : kritva::demo::Sensor
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include "sensor.hpp"

namespace kritva::demo {

core::Result<void> Sensor::on_configure(const core::Configuration& cfg) {
    const auto after = runtime::get_int(cfg, "sensor.failure_after_ticks", 0, 0, 1000000);
    if (!after) return core::Result<void>::failure(after.error());
    failure_after_ticks_ = after.value();
    return core::Result<void>::success();
}

bool Sensor::tick() {
    if (!in(core::LifecycleState::RUNNING)) return false;
    ++ticks_;
    reading_ = static_cast<std::int64_t>(ticks_) * 10;
    stats_.sample_count.increment();
    if (failure_after_ticks_ > 0 && ticks_ >= static_cast<std::uint64_t>(failure_after_ticks_)) {
        inject_failure("injected sensor failure");
    }
    return true;
}

void Sensor::inject_failure(const char* reason) {
    if (in(core::LifecycleState::FAULT)) return;
    enter_fault(reason);
    stats_.error_count.increment();
    if (sink_ != nullptr) {
        core::Event event;
        event.type = core::EventType::ERROR;
        event.severity = core::ErrorSeverity::ERROR;
        (void)core::runtime::ComponentEventReporter(*this, *sink_).report(event);
    }
}

} // namespace kritva::demo
