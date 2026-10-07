//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : nexus_edge_demo_hygiene_test.cpp
// Description : Proves that the reference demo goes through the production path and does not bypass it: RuntimeHost,
//               DeviceManager, the remote devices and endpoints (through the I3 interfaces), RemoteNode, LoopbackLink,
//               SimulatedTransport and EdgeHost; never the session layer, never a second driver, clock or manager.
//
// Component   : KritvaOS Examples
// Module      : Tests
// Layer       : Application
//
// Requirements: VR-001; VR-003; RI-001
// API         : NEXUS-EDGE-DEMO-HYGIENE-UT
//
// Author      : KritvaOS
// Created     : 07-10-2026
//==============================================================================

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../../runtime/check.hpp"

static std::string code_of(const std::string& path) {
    std::ifstream in(path);
    KRITVA_CHECK(in.good());
    std::string out, line;
    while (std::getline(in, line)) {
        const auto c = line.find("//");
        if (c != std::string::npos && line.find("\"") == std::string::npos) line.erase(c);   // comment-only lines; lines with strings are kept whole
        out += line + "\n";
    }
    return out;
}

static std::size_t count(const std::string& text, const std::string& token) {
    std::size_t n = 0;
    for (auto at = text.find(token); at != std::string::npos; at = text.find(token, at + token.size())) ++n;
    return n;
}

int main(int argc, char** argv) {
    KRITVA_CHECK(argc == 2);
    const std::string dir = std::string(argv[1]) + "/examples/nexus_edge_demo/";
    const std::vector<std::string> files = {"nexus_edge_demo_app.cpp", "nexus_edge_demo_app.hpp", "main.cpp"};
    std::string all;
    for (const auto& f : files) all += code_of(dir + f) + "\n";
    const std::string app = code_of(dir + "nexus_edge_demo_app.cpp");

    // The demo is made of the production path: each layer is used.
    for (const char* required : {"RuntimeHost", "DeviceManager", "RemoteNode", "LoopbackLink", "EdgeHost", "SimulatedTransport", "find_endpoint(", "host.initialize(", "host.start(",
                                 "host.stop(", "host.shutdown(", "host.configure(", "host.observe(", "manager.register_device(", "remote::diagnose(", "loop.advance(", "loop.attach(",
                                 "loop.peer_tick()", "dynamic_cast<hw::MotorCommandEndpoint*>", "dynamic_cast<hw::AccelerationEndpoint*>", "dynamic_cast<hw::PositionEndpoint*>",
                                 "RemoteNodeConfig", "link_timing_from("}) {
        if (app.find(required) == std::string::npos) { std::fprintf(stderr, "the demo does not use '%s'\n", required); return 1; }
    }

    // It never touches the session layer, never drives time or supervision itself, and has no second clock, thread, manager or recovery.
    const std::vector<std::string> forbidden = {
        "RemoteSession", "remote_session.hpp", "session()", "RemoteProxy", "RemoteMotorCommandEndpoint", "RemoteAccelerationEndpoint", "RemoteAngularVelocityEndpoint",
        "RemotePositionEndpoint", "retransmit", "enter_fault", "observe_edge",
        "service(", "poll(", "reopen(", ".open(", "close(", "send(", "receive(", "link.advance(", "link.connect(", ".reset(",
        "RemoteDeviceManager", "EdgeRuntime", "public core::runtime::Component", "public Component",
        "<chrono>", "<thread>", "<mutex>", "<atomic>", "<condition_variable>", "<future>", "<random>", "<ctime>", "sleep", "steady_clock", "system_clock", "clock_gettime",
        "gettimeofday", "std::thread", "std::async", "<sys/socket.h>", "<netinet", "socket(",
    };
    for (const auto& token : forbidden) {
        if (all.find(token) != std::string::npos) { std::fprintf(stderr, "the demo uses forbidden '%s'\n", token.c_str()); return 1; }
    }

    // The one driver of time is the LoopbackLink: every advance() is loop.advance().
    if (count(app, ".advance(") != count(app, "loop.advance(")) { std::fprintf(stderr, "the demo advances time other than through the LoopbackLink\n"); return 1; }
    // The only thing the application does with the simulated transport is configure the link and take it down, as the failure injection.
    if (count(app, "link.disconnect()") != 1) { std::fprintf(stderr, "expected exactly one injected link loss\n"); return 1; }

    // The Edge's mock hardware is observed, never controlled, outside the Edge: only effective_velocity() is read from it, and no
    // direct call reaches an Edge endpoint (all control goes through the DeviceManager's remote endpoints).
    std::size_t at = 0;
    while ((at = app.find(".command().", at)) != std::string::npos) {
        at += std::string(".command().").size();
        if (app.compare(at, std::string("effective_velocity(").size(), "effective_velocity(") != 0) { std::fprintf(stderr, "a direct call on an Edge endpoint\n"); return 1; }
    }
    for (const char* direct : {".acceleration().", ".angular_velocity().", ".position().", "inject_fault", "fail_next_operation", "motor.model()", "imu.acceleration("}) {
        if (app.find(direct) != std::string::npos) { std::fprintf(stderr, "a direct call on Edge hardware: %s\n", direct); return 1; }
    }
    std::printf("nexus_edge_demo_hygiene_test: PASS\n");
    return 0;
}
