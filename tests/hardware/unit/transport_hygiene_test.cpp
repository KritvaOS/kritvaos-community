//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : transport_hygiene_test.cpp
// Description : Checks that the transport sources use no wall clock, thread, sleep, hidden randomness
//               or real networking, and do not interpret frames.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: TR-001; TR-002
// API         : TRANSPORT-HYGIENE-UT
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

static std::string read(const std::string& path) {
    std::ifstream in(path);
    KRITVA_CHECK(in.good());
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Code only: comments are stripped so that a sentence like "no sleeping" does not trip the check.
static std::string code_of(const std::string& text) {
    std::string out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const auto c = line.find("//");
        if (c != std::string::npos) line.erase(c);
        out += line + "\n";
    }
    return out;
}

int main(int argc, char** argv) {
    KRITVA_CHECK(argc == 2);
    const std::string root = argv[1];
    const std::vector<std::string> files = {
        "hardware/transport/include/kritva/hardware/transport/transport.hpp",
        "hardware/transport/include/kritva/hardware/transport/simulated_transport.hpp",
        "hardware/transport/src/simulated_transport.cpp",
    };
    const std::vector<std::string> forbidden = {
        "<chrono>", "<thread>", "<mutex>", "<atomic>", "<condition_variable>", "<future>", "<random>", "<ctime>", "<time.h>",
        "sleep", "steady_clock", "system_clock", "high_resolution_clock", "clock_gettime", "gettimeofday", "std::time",
        "rand(", "random_device", "<sys/socket.h>", "<netinet", "<arpa/", "socket(", "<unistd.h>", "std::thread", "std::async",
    };
    // The transport carries bytes; it must not know the frame layout or the protocol.
    const std::vector<std::string> protocol_knowledge = {
        "decode_frame", "encode_frame", "FrameHeader", "kMagic", "kHeaderSize", "MessageType", "session", "correlation",
        "heartbeat", "codec.hpp", "frame.hpp", "protocol.hpp", "SessionState",
    };
    for (const auto& f : files) {
        const std::string code = code_of(read(root + "/" + f));
        for (const auto& token : forbidden) {
            if (code.find(token) != std::string::npos) {
                std::fprintf(stderr, "%s uses forbidden token '%s'\n", f.c_str(), token.c_str());
                return 1;
            }
        }
    }
    for (const auto& f : files) {
        std::string code = code_of(read(root + "/" + f));
        // simulated_transport.cpp may use kMaxFrameSize from protocol.hpp (the size bound is a transport concern).
        const std::string bound_include = "#include <kritva/hardware/transport/protocol.hpp>";
        const auto pos = code.find(bound_include);
        if (pos != std::string::npos) code.erase(pos, bound_include.size());
        for (const auto& token : protocol_knowledge) {
            if (code.find(token) != std::string::npos) {
                std::fprintf(stderr, "%s knows about the protocol: '%s'\n", f.c_str(), token.c_str());
                return 1;
            }
        }
    }
    std::printf("transport_hygiene_test: PASS\n");
    return 0;
}
