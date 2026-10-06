//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : remote_hygiene_test.cpp
// Description : Checks the boundaries of the Edge service sources: no runtime, DeviceManager, clock, thread or
//               networking use, and no concrete endpoint types outside the capability dispatch.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: ER-001; RI-001; RI-002
// API         : REMOTE-HYGIENE-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
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
        if (c != std::string::npos) line.erase(c);
        out += line + "\n";
    }
    return out;
}

static bool has(const std::string& code, const std::string& token) { return code.find(token) != std::string::npos; }

int main(int argc, char** argv) {
    KRITVA_CHECK(argc == 2);
    const std::string root = std::string(argv[1]) + "/hardware/remote/";
    const std::vector<std::string> files = {
        "include/kritva/hardware/remote/edge_host.hpp", "src/edge_host.cpp",
        "include/kritva/hardware/remote/capability_dispatch.hpp", "src/capability_dispatch.cpp",
    };
    const std::vector<std::string> forbidden = {
        // no Edge-side runtime, manager or Core component machinery
        "RuntimeHost", "RuntimeManager", "DeviceManager", "device_manager.hpp", "kritva/runtime/", "core/runtime", "Component",
        // no wall clock, threads, sleeping, randomness, sockets
        "<chrono>", "<thread>", "<mutex>", "<atomic>", "<random>", "<ctime>", "sleep", "steady_clock", "system_clock", "clock_gettime",
        "<sys/socket.h>", "<netinet", "socket(", "std::thread", "std::async",
    };
    for (const auto& f : files) {
        const std::string code = code_of(root + f);
        for (const auto& token : forbidden) {
            if (has(code, token)) {
                std::fprintf(stderr, "%s uses forbidden token '%s'\n", f.c_str(), token.c_str());
                return 1;
            }
        }
    }
    // The only place that knows concrete typed endpoint contracts and casts to them is the capability dispatch.
    const std::vector<std::string> concrete = {
        "dynamic_cast", "static_cast<AccelerationEndpoint", "ActuatorEndpoint", "SensorEndpoint", "MotorCommandEndpoint", "AccelerationEndpoint",
        "AngularVelocityEndpoint", "PositionEndpoint", "typed_endpoints.hpp", "samples.hpp", "MotorCommand", "AccelerationSample", "PositionSample",
        "mock_", "Mock",
    };
    for (const auto& f : {files[0], files[1]}) {
        const std::string code = code_of(root + f);
        for (const auto& token : concrete) {
            if (has(code, token)) {
                std::fprintf(stderr, "%s knows a concrete endpoint type: '%s' (only capability_dispatch may)\n", f.c_str(), token.c_str());
                return 1;
            }
        }
    }
    // Mocks are a test concern: the dispatch layer works on the I3 contracts only.
    for (const auto& f : {files[2], files[3]}) {
        const std::string code = code_of(root + f);
        if (has(code, "mock_") || has(code, "Mock")) {
            std::fprintf(stderr, "%s depends on a mock\n", f.c_str());
            return 1;
        }
    }
    // The Nexus side: it never depends on the Edge service (a peer in the same process is wired through the peer tick
    // only), on the runtime or the manager, and there is no RemoteDeviceManager.
    const std::vector<std::string> nexus_files = {
        "include/kritva/hardware/remote/remote_session.hpp", "src/remote_session.cpp",
        "include/kritva/hardware/remote/remote_node.hpp", "src/remote_node.cpp",
        "include/kritva/hardware/remote/remote_endpoint.hpp", "src/remote_endpoint.cpp",
        "include/kritva/hardware/remote/topology.hpp", "src/topology.cpp",
    };
    const std::vector<std::string> nexus_forbidden = {
        "EdgeHost", "edge_host.hpp", "RuntimeHost", "RuntimeManager", "DeviceManager", "device_manager.hpp", "RemoteDeviceManager",
        "kritva/runtime/", "core/runtime", "mock_", "Mock",
        "<chrono>", "<thread>", "<mutex>", "<atomic>", "<random>", "<ctime>", "sleep", "steady_clock", "system_clock", "clock_gettime",
        "<sys/socket.h>", "<netinet", "socket(", "std::thread", "std::async",
    };
    for (const auto& f : nexus_files) {
        const std::string code = code_of(root + f);
        for (const auto& token : nexus_forbidden) {
            if (has(code, token)) {
                std::fprintf(stderr, "%s uses forbidden token '%s'\n", f.c_str(), token.c_str());
                return 1;
            }
        }
    }
    // The Nexus diagnostics belong to the Nexus side too: no Edge object, and no loopback composition.
    for (const std::string f : {"include/kritva/hardware/remote/remote_diagnostics.hpp", "src/remote_diagnostics.cpp"}) {
        const std::string code = code_of(root + f);
        for (const auto& token : nexus_forbidden) if (has(code, token)) { std::fprintf(stderr, "%s uses forbidden token '%s'\n", f.c_str(), token.c_str()); return 1; }
    }
    for (const auto& f : nexus_files) {
        if (has(code_of(root + f), "loopback_link") || has(code_of(root + f), "LoopbackLink")) { std::fprintf(stderr, "%s depends on the loopback composition\n", f.c_str()); return 1; }
    }

    // Diagnostics are passive: no call that drives, sends, reads a frame, changes a lifecycle or a state, or recovers.
    const std::vector<std::string> active_calls = {
        "service(", "poll(", "advance(", "reopen(", "open(", "close(", "send(", "receive(", "initialize(", "start(", "stop(", "shutdown(",
        "configure(", "connect(", "disconnect(", "retransmit", "enter_fault", "fault(", "acquire(", "release(", "lifecycle(", "write(", "read(", "observe(",
        "set_handlers", "add_proxy", "clear(", "pop_back", "erase",
    };
    const std::vector<std::string> edge_side_forbidden = {
        "RemoteNode", "RemoteSession", "remote_node.hpp", "remote_session.hpp", "RuntimeHost", "RuntimeManager", "DeviceManager", "device_manager.hpp",
        "kritva/runtime/", "core/runtime", "mock_", "Mock", "<chrono>", "<thread>", "<mutex>", "<atomic>", "<random>", "<ctime>", "sleep", "steady_clock",
        "system_clock", "clock_gettime", "<sys/socket.h>", "socket(", "std::thread", "std::async",
    };
    const std::vector<std::string> diagnostics_files = {
        "include/kritva/hardware/remote/diagnostics_text.hpp", "include/kritva/hardware/remote/remote_diagnostics.hpp", "src/remote_diagnostics.cpp",
        "include/kritva/hardware/remote/edge_diagnostics.hpp", "src/edge_diagnostics.cpp",
    };
    for (const auto& f : diagnostics_files) {
        const std::string code = code_of(root + f);
        for (const auto& token : active_calls) {
            if (has(code, token)) { std::fprintf(stderr, "%s calls '%s': diagnostics must be passive\n", f.c_str(), token.c_str()); return 1; }
        }
        if (f.find("edge_diagnostics") != std::string::npos) {                       // the Edge diagnostics know no Nexus object
            for (const auto& token : edge_side_forbidden) if (has(code, token)) { std::fprintf(stderr, "%s uses forbidden token '%s'\n", f.c_str(), token.c_str()); return 1; }
        }
    }
    for (const auto& f : {"include/kritva/hardware/remote/diagnostics_text.hpp"}) {
        const std::string code = code_of(root + f);
        for (const auto& token : edge_side_forbidden) if (has(code, token)) { std::fprintf(stderr, "%s uses forbidden token '%s'\n", f, token.c_str()); return 1; }
    }

    // The LoopbackLink is a composition utility: it only advances the clock, polls the Edge and services the Nexus. It owns
    // nothing, is no Component, Device or Endpoint, has no clock or thread, and makes no lifecycle, safety, reconnect or recovery decision.
    const std::vector<std::string> loopback_files = {"include/kritva/hardware/remote/loopback_link.hpp", "src/loopback_link.cpp"};
    const std::vector<std::string> loopback_forbidden = {
        "RuntimeHost", "RuntimeManager", "DeviceManager", "device_manager.hpp", "kritva/runtime/", "core/runtime", "Component", ": public Endpoint", ": public Device",
        "unique_ptr", "shared_ptr", "make_unique", "make_shared", "new ", "RemoteSession", "mock_", "Mock",
        "initialize(", "start(", "stop(", "shutdown(", "configure(", "connect(", "disconnect(", "reopen(", "close(", "enter_fault", "fault(", "acquire(", "retransmit",
        "<chrono>", "<thread>", "<mutex>", "<atomic>", "<random>", "<ctime>", "sleep", "steady_clock", "system_clock", "clock_gettime", "<sys/socket.h>", "socket(", "std::thread", "std::async",
    };
    for (const auto& f : loopback_files) {
        const std::string code = code_of(root + f);
        for (const auto& token : loopback_forbidden) {
            if (has(code, token)) { std::fprintf(stderr, "%s uses forbidden token '%s'\n", f.c_str(), token.c_str()); return 1; }
        }
    }
    std::printf("remote_hygiene_test: PASS\n");
    return 0;
}
