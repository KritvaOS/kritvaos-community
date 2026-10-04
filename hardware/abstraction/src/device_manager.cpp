//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : device_manager.cpp
// Description : DeviceManager implementation.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Device Manager
// Layer       : Hardware Abstraction
//
// Requirements: DER-004; DER-205; DER-401..407; DER-601..607; DER-704
// API         : kritva::hardware::DeviceManager
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <kritva/hardware/device_manager.hpp>

#include <algorithm>
#include <cassert>
#include <variant>

namespace kritva::hardware {

using core::ErrorCode;
using core::HealthState;
using core::LifecycleState;
using core::Result;

DeviceManager::DeviceManager(core::runtime::ComponentId id)
    : Component(core::runtime::ComponentInfo::create(id, "device_manager").value()) {}

DeviceManager::~DeviceManager() { clear_listeners(); }

bool DeviceManager::enabled(const Device& device) const noexcept {
    return std::find(disabled_.begin(), disabled_.end(), &device) == disabled_.end();
}

Result<void> DeviceManager::with_source(Result<void> result) const {
    if (result) return result;
    core::Error e = result.error();
    e.source = info().id();
    return Result<void>::failure(std::move(e));
}

Result<void> DeviceManager::invalid_state(const char* operation) const {
    return Result<void>::failure(core::Error{ErrorCode::INVALID_STATE, core::ErrorSeverity::ERROR, info().id(), {},
                                             std::string(operation) + " is not valid in this lifecycle state"});
}

// A failing initialize/start/stop leaves the manager FAULT; the cause is returned with this component as source.
Result<void> DeviceManager::fail(Result<void> failed) {
    failed = with_source(std::move(failed));
    fault_detail_ = failed.error().message;
    const auto r = lifecycle_.transition_to(LifecycleState::FAULT);
    assert(r.has_value());
    (void)r;
    return failed;
}

std::vector<std::string> DeviceManager::config_keys() const {
    std::vector<std::string> keys;
    for (const Device* d : registry_.devices()) {
        keys.push_back(d->info().name() + ".enabled");
        for (const Endpoint* e : d->endpoints()) {
            for (const auto& setting : e->setting_names()) keys.push_back(d->info().name() + "." + e->info().name() + "." + setting);
        }
    }
    return keys;
}

// Visits the endpoints of the enabled devices: registration order, or exactly the reverse.
template <class F>
Result<void> DeviceManager::each_endpoint(bool reverse, F op) {
    std::vector<Endpoint*> order;
    for (Device* d : registry_.devices()) {
        if (!enabled(*d)) continue;
        for (Endpoint* e : d->endpoints()) order.push_back(e);
    }
    if (reverse) std::reverse(order.begin(), order.end());
    for (Endpoint* e : order) {
        if (auto r = op(*e); !r) return r;
    }
    return Result<void>::success();
}

Result<void> DeviceManager::configure(const core::Configuration& configuration) {
    const auto s = lifecycle_.state();
    if (s != LifecycleState::UNKNOWN && s != LifecycleState::STOPPED) return invalid_state("configure");

    // Validate every device switch before touching anything.
    std::vector<const Device*> disabled;
    for (const Device* d : registry_.devices()) {
        const core::Parameter* p = configuration.get(d->info().name() + ".enabled");
        if (p == nullptr) continue;
        const bool* flag = std::get_if<bool>(&p->value);
        if (flag == nullptr) {
            return with_source(Result<void>::failure(core::Error{ErrorCode::CONFIGURATION_ERROR, core::ErrorSeverity::ERROR, {}, {},
                                                                 "'" + d->info().name() + ".enabled' must be true or false"}));
        }
        if (!*flag) disabled.push_back(d);
    }
    disabled_ = std::move(disabled);

    for (Device* d : registry_.devices()) {
        if (!enabled(*d)) continue;
        for (Endpoint* e : d->endpoints()) {
            core::Configuration scoped;            // only this endpoint's settings, under their bare names
            for (const auto& setting : e->setting_names()) {
                if (const core::Parameter* p = configuration.get(d->info().name() + "." + e->info().name() + "." + setting)) {
                    (void)scoped.set(core::Parameter{setting, p->value, {}});
                }
            }
            if (auto r = e->configure(scoped); !r) return with_source(std::move(r));   // state unchanged
        }
    }
    return Result<void>::success();
}

void DeviceManager::install_listeners() {
    for (Device* d : registry_.devices()) {
        for (Endpoint* e : d->endpoints()) e->set_fault_listener([this](const Endpoint& ep) { on_endpoint_fault(ep); });
    }
}

void DeviceManager::clear_listeners() {
    for (Device* d : registry_.devices()) {
        for (Endpoint* e : d->endpoints()) e->set_fault_listener({});
    }
}

// A fault outside a manager lifecycle call is reported once; inside one, the call's own Error is the report.
void DeviceManager::on_endpoint_fault(const Endpoint&) {
    if (in_lifecycle_ || sink_ == nullptr) return;
    core::Event event;
    event.type = core::EventType::ERROR;
    event.severity = core::ErrorSeverity::ERROR;
    (void)core::runtime::ComponentEventReporter(*this, *sink_).report(event);
}

Result<void> DeviceManager::initialize() {
    const auto s = lifecycle_.state();
    if (s != LifecycleState::UNKNOWN && s != LifecycleState::STOPPED) return invalid_state("initialize");
    registry_.close();
    install_listeners();
    (void)lifecycle_.transition_to(LifecycleState::INITIALIZING);
    live_ = true;
    fault_detail_.clear();
    in_lifecycle_ = true;
    auto r = each_endpoint(false, [](Endpoint& e) { return e.initialize(); });
    in_lifecycle_ = false;
    if (!r) return fail(std::move(r));
    (void)lifecycle_.transition_to(LifecycleState::READY);
    return Result<void>::success();
}

Result<void> DeviceManager::start() {
    if (lifecycle_.state() != LifecycleState::READY) return invalid_state("start");
    in_lifecycle_ = true;
    auto r = each_endpoint(false, [](Endpoint& e) { return e.start(); });
    in_lifecycle_ = false;
    if (!r) return fail(std::move(r));
    (void)lifecycle_.transition_to(LifecycleState::RUNNING);
    return Result<void>::success();
}

Result<void> DeviceManager::stop() {
    const auto s = lifecycle_.state();
    if (s != LifecycleState::READY && s != LifecycleState::RUNNING) return invalid_state("stop");
    if (s == LifecycleState::RUNNING) (void)lifecycle_.transition_to(LifecycleState::STOPPING);
    in_lifecycle_ = true;
    // Faulted endpoints are not stopped: shutdown() releases them.
    auto r = each_endpoint(true, [](Endpoint& e) {
        const auto st = e.lifecycle_state();
        return (st == LifecycleState::READY || st == LifecycleState::RUNNING) ? e.stop() : Result<void>::success();
    });
    in_lifecycle_ = false;
    if (!r) return fail(std::move(r));
    (void)lifecycle_.transition_to(LifecycleState::STOPPED);
    return Result<void>::success();
}

Result<void> DeviceManager::shutdown() {
    const auto s = lifecycle_.state();
    if (s != LifecycleState::UNKNOWN && s != LifecycleState::STOPPED && s != LifecycleState::FAULT) return invalid_state("shutdown");
    if (live_) {
        in_lifecycle_ = true;
        Result<void> first = Result<void>::success();
        // Best effort over all endpoints so one failure cannot strand the others; the first error is returned.
        (void)each_endpoint(true, [&](Endpoint& e) {
            const auto st = e.lifecycle_state();
            if (st == LifecycleState::READY || st == LifecycleState::RUNNING) (void)e.stop();
            if (auto r = e.shutdown(); !r && first) first = std::move(r);
            return Result<void>::success();
        });
        in_lifecycle_ = false;
        if (!first) return with_source(std::move(first));       // state unchanged; a retry resumes
        live_ = false;
    }
    if (s == LifecycleState::FAULT) {
        (void)lifecycle_.transition_to(LifecycleState::STOPPED);
        fault_detail_.clear();
    }
    return Result<void>::success();
}

std::vector<DeviceDiagnostics> DeviceManager::diagnostics() const {
    const auto names = [](const core::CapabilitySet& set) {
        std::vector<std::string> out;
        for (const auto& c : set.all()) out.push_back(c.name);
        return out;
    };
    std::vector<DeviceDiagnostics> all;
    for (const Device* d : registry_.devices()) {
        DeviceDiagnostics dd;
        dd.id = d->info().id();
        dd.name = d->info().name();
        dd.enabled = enabled(*d);
        dd.status = d->status().code();
        const auto health = d->health();
        dd.health = health.state();
        dd.health_detail = health.detail();
        dd.capabilities = names(d->capabilities());
        for (const Endpoint* e : d->endpoints()) {
            EndpointDiagnostics ed;
            ed.device_id = dd.id;
            ed.device_name = dd.name;
            ed.endpoint_id = e->info().id();
            ed.endpoint_name = e->info().name();
            ed.direction = e->info().direction();
            ed.enabled = dd.enabled;
            ed.lifecycle = e->lifecycle_state();
            ed.status = e->status().code();
            const auto eh = e->health();
            ed.health = eh.state();
            ed.health_detail = eh.detail();
            ed.operations_ok = e->statistics().sample_count.value();
            ed.operations_failed = e->statistics().error_count.value();
            ed.last_error = e->last_error();
            ed.capabilities = names(e->capabilities());
            dd.endpoints.push_back(std::move(ed));
        }
        all.push_back(std::move(dd));
    }
    return all;
}

core::Status DeviceManager::status() const {
    if (lifecycle_.state() == LifecycleState::FAULT) return core::Status{core::StatusCode::FAILED};
    if (lifecycle_.state() == LifecycleState::UNKNOWN) return core::Status{core::StatusCode::UNKNOWN};
    for (const Device* d : registry_.devices()) {
        if (enabled(*d) && d->status().code() == core::StatusCode::FAILED) return core::Status{core::StatusCode::FAILED};
    }
    return core::Status{core::StatusCode::OK};
}

core::Health DeviceManager::health() const {
    if (lifecycle_.state() == LifecycleState::FAULT) {
        core::Health h{HealthState::UNHEALTHY};
        h.set_detail(fault_detail_);
        return h;
    }
    const Device* degraded = nullptr;
    for (const Device* d : registry_.devices()) {
        if (!enabled(*d)) continue;
        const auto h = d->health();
        if (h.state() == HealthState::UNHEALTHY) {
            core::Health out{HealthState::UNHEALTHY};
            out.set_detail(d->info().name() + "." + h.detail());
            return out;
        }
        if (h.state() == HealthState::DEGRADED && degraded == nullptr) degraded = d;
    }
    if (degraded != nullptr) {
        core::Health out{HealthState::DEGRADED};
        out.set_detail(degraded->info().name() + "." + degraded->health().detail());
        return out;
    }
    return core::Health{lifecycle_.state() == LifecycleState::RUNNING ? HealthState::HEALTHY : HealthState::UNKNOWN};
}

core::CapabilitySet DeviceManager::capabilities() const {
    core::CapabilitySet all;
    for (const Device* d : registry_.devices()) {
        if (!enabled(*d)) continue;
        const auto device_caps = d->capabilities();   // keep the temporary alive for the loop
        for (const auto& c : device_caps.all()) all.add(c);
    }
    return all;
}

core::Statistics DeviceManager::statistics() const {
    core::Statistics total;
    for (const Device* d : registry_.devices()) {
        if (!enabled(*d)) continue;
        for (const Endpoint* e : d->endpoints()) {
            total.sample_count.increment(e->statistics().sample_count.value());
            total.error_count.increment(e->statistics().error_count.value());
        }
    }
    return total;
}

} // namespace kritva::hardware
