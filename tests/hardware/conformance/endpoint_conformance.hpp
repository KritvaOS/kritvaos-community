//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : endpoint_conformance.hpp
// Description : Reusable conformance checks of the Endpoint, SensorEndpoint and ActuatorEndpoint contracts.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: DER-301..307; DER-401..404; DER-503; DER-504; DER-703; DER-801
// API         : check_endpoint_contract / check_sensor_contract / check_actuator_contract
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <kritva/hardware/actuator_endpoint.hpp>
#include <kritva/hardware/sensor_endpoint.hpp>

// Each check returns the first violation of the contract, or an empty string. Any
// Endpoint implementation (the I3-001 test doubles, the I3-005 mocks, a future
// driver) is expected to pass them unchanged.
namespace kritva::hardware::conformance {

using core::ErrorCode;
using core::LifecycleState;
using LS = LifecycleState;

template <class E>
struct Fixture {
    std::function<std::unique_ptr<E>()> make;
    /// Moves a live endpoint to FAULT, as a real failure would.
    std::function<core::Result<void>(E&)> induce_fault;
};

namespace detail {

[[nodiscard]] inline const char* name(LS s) {
    switch (s) {
        case LS::UNKNOWN: return "UNKNOWN"; case LS::READY: return "READY"; case LS::RUNNING: return "RUNNING";
        case LS::STOPPED: return "STOPPED"; case LS::FAULT: return "FAULT"; default: return "other";
    }
}

/// Drives a fresh endpoint to `target`; returns an empty string or the failing step.
template <class E>
std::string reach(E& e, LS target, const Fixture<E>& f) {
    const auto step = [&](const char* what, core::Result<void> r) { return r ? std::string{} : std::string("cannot ") + what; };
    std::string err;
    if (target == LS::UNKNOWN) return err;
    if (!(err = step("initialize", e.initialize())).empty()) return err;
    if (target == LS::READY) return err;
    if (target == LS::STOPPED) {
        if (!(err = step("start", e.start())).empty()) return err;
        return step("stop", e.stop());
    }
    if (!(err = step("start", e.start())).empty()) return err;
    if (target == LS::RUNNING) return err;
    return step("induce fault", f.induce_fault(e));
}

} // namespace detail

template <class E>
std::string check_endpoint_contract(const Fixture<E>& f, EndpointDirection direction) {
    using detail::name; using detail::reach;
    {   // identity, initial observation
        auto e = f.make();
        if (!e->info().id().valid() || !valid_name(e->info().name())) return "endpoint identity is not valid";
        if (e->info().direction() != direction) return "endpoint direction is wrong";
        if (e->lifecycle_state() != LS::UNKNOWN) return "a new endpoint is not UNKNOWN";
        if (e->status().code() != core::StatusCode::UNKNOWN) return "a new endpoint status is not UNKNOWN";
        if (e->health().state() != core::HealthState::UNKNOWN) return "a new endpoint health is not UNKNOWN";
        if (e->statistics().sample_count.value() != 0 || e->statistics().error_count.value() != 0) return "new statistics are not zero";
        if (e->last_error()) return "a new endpoint has a last error";
        if (e->capabilities().empty()) return "an endpoint exposes no capability";
    }
    // Exhaustive lifecycle table: every operation from every reachable state.
    enum Op { CONFIGURE, INITIALIZE, START, STOP, SHUTDOWN };
    const char* op_name[] = {"configure", "initialize", "start", "stop", "shutdown"};
    const LS states[] = {LS::UNKNOWN, LS::READY, LS::RUNNING, LS::STOPPED, LS::FAULT};
    // valid[state][op], and the state after a valid operation (UNKNOWN entries mean "unchanged").
    const bool valid[5][5] = {
        /* UNKNOWN */ {true, true, false, false, true},
        /* READY   */ {false, false, true, true, false},
        /* RUNNING */ {false, false, false, true, false},
        /* STOPPED */ {true, true, false, false, true},
        /* FAULT   */ {false, false, false, false, true}};
    for (int si = 0; si < 5; ++si) {
        for (int op = 0; op < 5; ++op) {
            auto e = f.make();
            if (auto err = reach(*e, states[si], f); !err.empty()) return std::string(name(states[si])) + ": " + err;
            core::Configuration cfg;
            core::Result<void> r = core::Result<void>::success();
            switch (op) {
                case CONFIGURE: r = e->configure(cfg); break; case INITIALIZE: r = e->initialize(); break;
                case START: r = e->start(); break; case STOP: r = e->stop(); break; default: r = e->shutdown(); break;
            }
            const std::string where = std::string(op_name[op]) + " from " + name(states[si]);
            if (valid[si][op]) {
                if (!r) return where + " must succeed";
                LS expected = states[si];
                if (op == INITIALIZE) expected = LS::READY;
                if (op == START) expected = LS::RUNNING;
                if (op == STOP) expected = LS::STOPPED;
                if (op == SHUTDOWN && states[si] == LS::FAULT) expected = LS::STOPPED;
                if (e->lifecycle_state() != expected) return where + " reached " + name(e->lifecycle_state());
            } else {
                if (r || r.error().code != ErrorCode::INVALID_STATE) return where + " must fail with INVALID_STATE";
                if (r.error().source.valid()) return where + ": an endpoint must not set an error source";
                if (e->lifecycle_state() != states[si]) return where + " changed the state";
                if (!e->last_error() || e->last_error()->code != ErrorCode::INVALID_STATE) return where + " did not record last_error";
            }
        }
    }
    {   // observation per state, restart, and no silent recovery
        auto e = f.make();
        if (auto err = reach(*e, LS::RUNNING, f); !err.empty()) return err;
        if (e->health().state() != core::HealthState::HEALTHY || e->status().code() != core::StatusCode::OK) return "a RUNNING endpoint is not HEALTHY/OK";
        if (!f.induce_fault(*e)) return "cannot fault a RUNNING endpoint";
        if (e->lifecycle_state() != LS::FAULT) return "a faulted endpoint is not FAULT";
        if (e->health().state() != core::HealthState::UNHEALTHY || e->health().detail().empty()) return "a faulted endpoint is not UNHEALTHY with a detail";
        if (e->status().code() != core::StatusCode::FAILED) return "a faulted endpoint status is not FAILED";
        if (!e->last_error()) return "a faulted endpoint has no last error";
        if (!f.induce_fault(*e)) return "faulting an already faulted endpoint must be idempotent";
        for (int i = 0; i < 3; ++i) {                              // DER-703: nothing restarts it
            core::Configuration cfg;
            (void)e->configure(cfg); (void)e->initialize(); (void)e->start(); (void)e->stop();
            if (e->lifecycle_state() != LS::FAULT || e->health().state() != core::HealthState::UNHEALTHY) return "a faulted endpoint recovered by itself";
        }
        if (!e->shutdown() || e->lifecycle_state() != LS::STOPPED) return "shutdown must release a faulted endpoint to STOPPED";
        if (e->health().state() != core::HealthState::UNKNOWN) return "a STOPPED endpoint health is not UNKNOWN";
        if (!e->initialize() || !e->start() || e->lifecycle_state() != LS::RUNNING) return "an endpoint cannot be restarted explicitly after shutdown";
        if (e->health().state() != core::HealthState::HEALTHY) return "a restarted endpoint is not HEALTHY";
    }
    {   // setting names are valid and unique
        auto e = f.make();
        std::vector<std::string> seen;
        for (const auto& n : e->setting_names()) {
            if (!valid_name(n)) return "a setting name is not a valid name";
            for (const auto& other : seen) if (other == n) return "setting names are not unique";
            seen.push_back(n);
        }
    }
    {   // the fault listener fires exactly once per fault, with the faulting endpoint
        auto e = f.make();
        int calls = 0;
        const Endpoint* seen = nullptr;
        e->set_fault_listener(&calls, [&](const Endpoint& ep) { ++calls; seen = &ep; });
        if (auto err = reach(*e, LS::RUNNING, f); !err.empty()) return err;
        if (calls != 0) return "the fault listener fired without a fault";
        if (!f.induce_fault(*e) || calls != 1 || seen != e.get()) return "the fault listener must fire once, with the endpoint";
        if (!f.induce_fault(*e) || calls != 1) return "the fault listener must not fire again while faulted";
        if (!e->shutdown() || calls != 1) return "shutdown must not fire the fault listener";
        if (!e->initialize() || !e->start() || !f.induce_fault(*e) || calls != 2) return "a new fault after restart must fire the listener again";
        e->clear_fault_listener(&calls);
        if (!e->shutdown()) return "shutdown failed";
    }
    {   // a fault is only possible on a live endpoint
        auto e = f.make();
        if (f.induce_fault(*e)) return "faulting a never-initialized endpoint must fail";
        if (e->lifecycle_state() != LS::UNKNOWN) return "a rejected fault changed the state";
    }
    return {};
}

template <class E, class Operation>
std::string check_data_operation(const Fixture<E>& f, const Operation& op, const char* what) {
    using detail::reach; using detail::name;
    for (LS s : {LS::UNKNOWN, LS::READY, LS::STOPPED}) {
        auto e = f.make();
        if (auto err = reach(*e, s, f); !err.empty()) return err;
        const auto r = op(*e);
        if (r || r.error().code != ErrorCode::NOT_READY) return std::string(what) + " from " + name(s) + " must fail with NOT_READY";
        if (e->statistics().error_count.value() != 1 || e->statistics().sample_count.value() != 0) return std::string(what) + ": a refused operation is not counted as an error";
        if (!e->last_error() || e->last_error()->code != ErrorCode::NOT_READY) return std::string(what) + ": last_error not recorded";
        if (r.error().source.valid()) return std::string(what) + ": an endpoint must not set an error source";
    }
    {
        auto e = f.make();
        if (auto err = reach(*e, LS::FAULT, f); !err.empty()) return err;
        const auto r = op(*e);
        if (r || r.error().code != ErrorCode::RESOURCE_UNAVAILABLE) return std::string(what) + " on a FAULT endpoint must fail with RESOURCE_UNAVAILABLE";
        if (e->lifecycle_state() != LS::FAULT) return std::string(what) + " changed a FAULT endpoint";
    }
    {
        auto e = f.make();
        if (auto err = reach(*e, LS::RUNNING, f); !err.empty()) return err;
        if (!op(*e)) return std::string(what) + " must succeed while RUNNING";
        if (e->statistics().sample_count.value() != 1 || e->statistics().error_count.value() != 0) return std::string(what) + ": a successful operation is not counted";
        if (e->lifecycle_state() != LS::RUNNING) return std::string(what) + " changed the state";
    }
    return {};
}

template <class Sample>
std::string check_sensor_contract(const Fixture<SensorEndpoint<Sample>>& f) {
    if (auto v = check_endpoint_contract(f, EndpointDirection::SENSOR); !v.empty()) return v;
    return check_data_operation(f, [](SensorEndpoint<Sample>& e) { Sample s{}; return e.read(s); }, "read");
}

template <class Command>
std::string check_actuator_contract(const Fixture<ActuatorEndpoint<Command>>& f) {
    if (auto v = check_endpoint_contract(f, EndpointDirection::ACTUATOR); !v.empty()) return v;
    return check_data_operation(f, [](ActuatorEndpoint<Command>& e) { return e.write(Command{}); }, "write");
}

} // namespace kritva::hardware::conformance
