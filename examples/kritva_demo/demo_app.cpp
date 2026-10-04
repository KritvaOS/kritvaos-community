//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : demo_app.cpp
// Description : Reference application implementation.
//
// Component   : KritvaOS Demo
// Module      : Reference Demo
// Layer       : Application
//
// Requirements: RR-APP-001..006; RR-TST-007
// API         : kritva::demo::DemoApplication
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include "demo_app.hpp"

#include <string>

#include <kritva/runtime/runtime_host.hpp>

#include "controller.hpp"
#include "monitor.hpp"
#include "sensor.hpp"

namespace kritva::demo {

namespace {

using runtime::to_string;

std::string names(const runtime::RuntimeObservation& o, const std::vector<core::runtime::ComponentId>& ids) {
    std::string out;
    for (const auto id : ids) {
        for (const auto& c : o.components) {
            if (c.observation.id == id) out += (out.empty() ? "" : ",") + c.name;
        }
    }
    return out.empty() ? "none" : out;
}

} // namespace

DemoOutcome DemoApplication::run(const core::Configuration& configuration) {
    const auto fail = [&](const std::string& what, const core::Error& e) {
        out_ << "[kritva_demo] " << what << ": " << e.message << "\n";
        return DemoOutcome::ERROR;
    };

    // Application-level switches, validated before anything is composed.
    const auto sensor_on = runtime::get_bool(configuration, "sensor.enabled", true);
    const auto controller_on = runtime::get_bool(configuration, "controller.enabled", true);
    const auto monitor_on = runtime::get_bool(configuration, "monitor.enabled", true);
    const auto ticks = runtime::get_int(configuration, "demo.ticks", 5, 1, 10000);
    for (const auto* r : {&sensor_on, &controller_on, &monitor_on}) if (!*r) return fail("CONFIG ERROR", r->error());
    if (!ticks) return fail("CONFIG ERROR", ticks.error());

    runtime::RuntimeHost host;
    runtime::EventLog events;
    host.set_event_sink(&events);

    Sensor sensor;
    Controller controller(sensor);
    Monitor monitor(host);
    sensor.set_event_sink(&events);

    const core::runtime::ComponentId sensor_id{Sensor::kId};
    const core::runtime::ComponentId controller_id{Controller::kId};

    // Compose. The monitor depends on whichever of sensor/controller are present.
    std::string composed;
    if (sensor_on.value()) {
        if (auto r = host.add_component(sensor); !r) return fail("COMPOSE ERROR", r.error());
        composed += " sensor";
    }
    if (controller_on.value()) {
        if (auto r = host.add_component(controller, {sensor_id}); !r) return fail("COMPOSE ERROR", r.error());
        composed += " controller";
    }
    if (monitor_on.value()) {
        core::Result<void> r = core::Result<void>::success();
        if (sensor_on.value() && controller_on.value()) r = host.add_component(monitor, {sensor_id, controller_id});
        else if (sensor_on.value()) r = host.add_component(monitor, {sensor_id});
        else if (controller_on.value()) r = host.add_component(monitor, {controller_id});
        else r = host.add_component(monitor);
        if (!r) return fail("COMPOSE ERROR", r.error());
        composed += " monitor";
    }

    if (auto r = host.configure(configuration); !r) return fail("CONFIG ERROR", r.error());
    out_ << "[kritva_demo] config: "
         << runtime::describe_configuration(host.settings(), configuration.size()) << "\n";
    out_ << "[kritva_demo] composed:" << composed << "\n";
    out_ << "[kritva_demo] state=" << to_string(host.state()) << "\n";

    const runtime::StatisticsProviders providers{
        {core::runtime::ComponentId{Sensor::kId}, &sensor},
        {core::runtime::ComponentId{Controller::kId}, &controller},
        {core::runtime::ComponentId{Monitor::kId}, &monitor}};

    if (auto r = host.initialize(); !r) {
        (void)host.controlled_shutdown();
        return fail("INITIALIZE FAILED", r.error());
    }
    out_ << "[kritva_demo] state=" << to_string(host.state()) << "\n";
    if (auto r = host.start(); !r) {
        (void)host.controlled_shutdown();
        return fail("START FAILED", r.error());
    }
    out_ << "[kritva_demo] state=" << to_string(host.state()) << "\n";

    // Simulated application activity: one deterministic step per tick, in dependency order.
    bool failure = false;
    for (std::int64_t i = 1; i <= ticks.value() && !failure; ++i) {
        sensor.tick();
        controller.tick();
        monitor.tick();
        out_ << "[kritva_demo] tick " << i << " sensor=" << sensor.reading() << " command=" << controller.command()
             << " failed=" << monitor.failed().size() << "\n";
        failure = monitor.failure_observed();
    }

    const auto observation = host.observe(providers);
    if (observation) out_ << runtime::describe(observation.value());

    if (failure) {
        if (observation) {
            out_ << "[kritva_demo] FAILURE observed: failed=" << names(observation.value(), monitor.failed())
                 << " affected=" << names(observation.value(), monitor.affected()) << "\n";
        }
        for (const auto& e : events.snapshot()) {
            if (e.type == core::EventType::ERROR) out_ << "[kritva_demo] event ERROR source=" << e.source_id.value() << "\n";
        }
        (void)host.controlled_shutdown();
        out_ << "[kritva_demo] state=" << to_string(host.state()) << "\n";
        out_ << "[kritva_demo] controlled shutdown complete (events=" << events.size() << ")\n";
        return host.state() == core::LifecycleState::STOPPED ? DemoOutcome::FAILURE_HANDLED : DemoOutcome::ERROR;
    }

    if (auto r = host.stop(); !r) {
        (void)host.controlled_shutdown();
        return fail("STOP FAILED", r.error());
    }
    out_ << "[kritva_demo] state=" << to_string(host.state()) << "\n";
    if (auto r = host.shutdown(); !r) return fail("SHUTDOWN FAILED", r.error());
    out_ << "[kritva_demo] shutdown complete (events=" << events.size() << ")\n";
    return DemoOutcome::CLEAN;
}

} // namespace kritva::demo
