//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : runtime_host.cpp
// Description : Application runtime host implementation.
//
// Component   : KritvaOS Runtime
// Module      : Runtime Host
// Layer       : Application Runtime
//
// Requirements: RR-LIF-001..008; RR-FLT-006
// API         : kritva::runtime::RuntimeHost
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <kritva/runtime/runtime_host.hpp>

namespace kritva::runtime {

using core::LifecycleState;
using core::Result;

std::string_view to_string(LifecycleState state) noexcept {
    switch (state) {
        case LifecycleState::UNKNOWN:      return "UNKNOWN";
        case LifecycleState::INITIALIZING: return "INITIALIZING";
        case LifecycleState::READY:        return "READY";
        case LifecycleState::RUNNING:      return "RUNNING";
        case LifecycleState::STOPPING:     return "STOPPING";
        case LifecycleState::STOPPED:      return "STOPPED";
        case LifecycleState::FAULT:        return "FAULT";
        case LifecycleState::RECOVERING:   return "RECOVERING";
    }
    return "INVALID";
}

Result<void> RuntimeHost::run(const StateObserver& observer) {
    const auto notify = [&] { if (observer) observer(manager_.state()); };

    // A failed step leaves the runtime in FAULT; reset() (FAULT -> STOPPED)
    // stops and shuts down the components. The original error is returned.
    const auto fail = [&](Result<void> failed) {
        if (manager_.state() == LifecycleState::FAULT) (void)manager_.reset();
        (void)manager_.shutdown();
        return failed;
    };

    if (auto r = manager_.initialize(); !r) return fail(r);
    notify();
    if (auto r = manager_.start(); !r) return fail(r);
    notify();
    if (auto r = manager_.stop(); !r) return fail(r);
    notify();
    return manager_.shutdown();
}

} // namespace kritva::runtime
