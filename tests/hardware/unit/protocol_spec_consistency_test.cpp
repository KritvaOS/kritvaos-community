//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : protocol_spec_consistency_test.cpp
// Description : Checks the contract headers against docs/architecture/KOS-I4_PROTOCOL.md so code and specification cannot drift.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: FR-001; FR-002; PR-001; PR-002; TR-003
// API         : PROTOCOL-SPEC-CONSISTENCY
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <cstdint>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "../../runtime/check.hpp"

#include <kritva/hardware/transport/protocol.hpp>

using namespace kritva::hardware::transport;

// Usage: protocol_spec_consistency_test <path to KOS-I4_PROTOCOL.md>
int main(int argc, char** argv) {
    KRITVA_CHECK(argc == 2);
    std::ifstream in(argv[1]);
    KRITVA_CHECK(in.good());
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string doc = buffer.str();

    const auto first_number = [](const std::string& text) -> std::uint64_t {      // first run of digits in a table cell
        std::smatch m;
        KRITVA_CHECK(std::regex_search(text, m, std::regex("[0-9]+")));
        return std::stoull(m.str());
    };
    std::vector<std::vector<std::string>> rows;                                  // every markdown table row, split into cells
    std::istringstream lines(doc);
    for (std::string line; std::getline(lines, line);) {
        if (line.rfind("| ", 0) != 0) continue;
        std::vector<std::string> cells;
        std::string cell;
        std::istringstream row(line.substr(1));
        while (std::getline(row, cell, '|')) {
            const auto a = cell.find_first_not_of(' ');
            const auto b = cell.find_last_not_of(' ');
            cells.push_back(a == std::string::npos ? "" : cell.substr(a, b - a + 1));
        }
        rows.push_back(cells);
    }

    // ---- constants (section 3) ----
    const std::map<std::string, std::uint64_t> constants{
        {"`PROTOCOL_MAJOR` / `PROTOCOL_MINOR`", 0}, {"`HEADER_SIZE`", kHeaderSize}, {"`MAX_FRAME_SIZE`", kMaxFrameSize},
        {"`MAX_PAYLOAD_SIZE`", kMaxPayloadSize}, {"`MAX_NAME_LENGTH`", kMaxNameLength}, {"`MAX_TEXT_LENGTH`", kMaxTextLength},
        {"`MAX_DEVICES`", kMaxDevices}, {"`MAX_DISCOVERY_ITEMS`", kMaxDiscoveryItems},
        {"`MAX_CAPABILITIES_PER_ENDPOINT`", kMaxCapabilitiesPerEndpoint}, {"`MAX_CONFIGURE_SETTINGS`", kMaxConfigureSettings},
        {"`MAX_DISCOVERY_PAYLOAD`", kMaxDiscoveryPayload}};
    int constants_seen = 0;
    for (const auto& r : rows) {
        if (r.size() < 2) continue;
        const auto it = constants.find(r[0]);
        if (it == constants.end() || r[0] == "`PROTOCOL_MAJOR` / `PROTOCOL_MINOR`") continue;
        KRITVA_CHECK(first_number(r[1]) == it->second);
        ++constants_seen;
    }
    KRITVA_CHECK(constants_seen == 10);
    KRITVA_CHECK(doc.find("| `MAGIC` | `0x4B344F53`") != std::string::npos && kMagic == 0x4B344F53);
    KRITVA_CHECK(doc.find("| `PROTOCOL_MAJOR` / `PROTOCOL_MINOR` | 1 / 0 |") != std::string::npos && kProtocolMajor == 1 && kProtocolMinor == 0);

    // ---- header layout (section 4): offset, size, field ----
    const std::map<std::string, std::pair<std::size_t, std::size_t>> layout{
        {"`magic`", {kOffsetMagic, 4}}, {"`protocol_major`", {kOffsetMajor, 2}}, {"`protocol_minor`", {kOffsetMinor, 2}},
        {"`message_type`", {kOffsetType, 2}}, {"`flags`", {kOffsetFlags, 2}}, {"`header_length`", {kOffsetHeaderLength, 2}},
        {"`reserved`", {kOffsetReserved, 2}}, {"`payload_length`", {kOffsetPayloadLength, 4}}, {"`sequence`", {kOffsetSequence, 8}},
        {"`correlation_id`", {kOffsetCorrelation, 8}}, {"`session_id`", {kOffsetSession, 8}}};
    int fields_seen = 0;
    std::size_t end = 0;
    for (const auto& r : rows) {
        if (r.size() < 3) continue;
        const auto it = layout.find(r[2]);
        if (it == layout.end()) continue;
        KRITVA_CHECK(first_number(r[0]) == it->second.first && first_number(r[1]) == it->second.second);
        end = std::max(end, it->second.first + it->second.second);
        ++fields_seen;
    }
    KRITVA_CHECK(fields_seen == 11 && end == kHeaderSize);

    // ---- message types (section 6) ----
    std::map<std::uint16_t, std::string> doc_types;
    for (const auto& r : rows) {
        if (r.size() < 4 || r[0].rfind("0x", 0) != 0) continue;
        const auto value = static_cast<std::uint16_t>(std::stoul(r[0], nullptr, 16));
        KRITVA_CHECK(doc_types.emplace(value, r[1]).second);                     // no duplicate type numbers in the spec
        const auto type = message_type_from_wire(value);
        KRITVA_CHECK(type.has_value() && r[1] == message_type_name(*type));
        const std::string dir = r[2];
        const auto d = direction_of(*type);
        KRITVA_CHECK((dir == "Nexus to Edge" && d == Direction::NEXUS_TO_EDGE) || (dir == "Edge to Nexus" && d == Direction::EDGE_TO_NEXUS) ||
                     (dir == "both" && d == Direction::BOTH));
        const auto response = response_for(*type);
        if (response.has_value()) KRITVA_CHECK(r[3] == message_type_name(*response));
        else KRITVA_CHECK(r[3].empty() || r[3] == "none");
    }
    KRITVA_CHECK(doc_types.size() == kAllMessageTypes.size());                   // the spec lists every type and only those

    // ---- status mapping (section 8): wire value, Core ErrorCode name ----
    const char* names[] = {"NONE", "UNKNOWN", "INVALID_ARGUMENT", "INVALID_STATE", "NOT_INITIALIZED", "NOT_READY", "ALREADY_RUNNING",
                           "TIMEOUT", "RESOURCE_UNAVAILABLE", "CONFIGURATION_ERROR", "UNSUPPORTED", "INTERNAL_ERROR"};
    int status_seen = 0;
    for (const auto& r : rows) {
        if (r.size() < 3 || r[1].size() < 3 || r[1].front() != '`') continue;
        const std::string code = r[1].substr(1, r[1].size() - 2);
        for (unsigned v = 0; v <= kMaxWireStatus; ++v) {
            if (code == names[v] && r[0] == std::to_string(v)) { KRITVA_CHECK(to_wire_status(static_cast<kritva::core::ErrorCode>(v)) == v); ++status_seen; }
        }
    }
    KRITVA_CHECK(status_seen == 12);

    // ---- session states and transitions (section 9) ----
    const std::map<std::string, SessionState> states{{"DISCONNECTED", SessionState::DISCONNECTED}, {"CONNECTING", SessionState::CONNECTING},
        {"NEGOTIATING", SessionState::NEGOTIATING}, {"CONNECTED", SessionState::CONNECTED}, {"DEGRADED", SessionState::DEGRADED}};
    int transitions_seen = 0;
    for (const auto& r : rows) {
        if (r.size() < 3 || !states.count(r[0]) || !states.count(r[1])) continue;
        KRITVA_CHECK(is_valid_transition(states.at(r[0]), states.at(r[1])));
        ++transitions_seen;
    }
    KRITVA_CHECK(transitions_seen == 9);                                         // the table lists exactly the valid transitions
    int valid_in_code = 0;
    for (const auto& a : states) for (const auto& b : states) if (is_valid_transition(a.second, b.second)) ++valid_in_code;
    KRITVA_CHECK(valid_in_code == transitions_seen);

    // ---- timing keys (section 14): key, default ----
    const LinkTiming defaults;
    const std::map<std::string, std::uint64_t> timing{{"`link.heartbeat_period_ms`", defaults.heartbeat_period_ms},
        {"`link.heartbeat_timeout_ms`", defaults.heartbeat_timeout_ms}, {"`link.request_timeout_ms`", defaults.request_timeout_ms},
        {"`link.pump_quantum_ms`", defaults.pump_quantum_ms}};
    int timing_seen = 0;
    for (const auto& r : rows) {
        if (r.size() < 3) continue;
        const auto it = timing.find(r[0]);
        if (it == timing.end()) continue;
        KRITVA_CHECK(first_number(r[1]) == it->second);
        ++timing_seen;
    }
    KRITVA_CHECK(timing_seen == 4);

    // The derivation in section 13 gives the same number as the code, and the spec states the wording the architect review required.
    KRITVA_CHECK(doc.find("`2 + 2 + 4864 + 38656 = 43524` bytes") != std::string::npos);
    KRITVA_CHECK(doc.find("at-most-once application at the Edge") != std::string::npos);
    KRITVA_CHECK(doc.find("**DEGRADED never stops any actuator by itself**") != std::string::npos);
    KRITVA_CHECK(doc.find("This is an Edge-side safety action, not only an identifier replacement") != std::string::npos);
    KRITVA_CHECK(doc.find("per peer direction and per session") != std::string::npos);
    KRITVA_CHECK(doc.find("do not impose an order between independently outstanding **responses**") != std::string::npos);
    KRITVA_CHECK(doc.find("it is **not** rejected because its `sequence` is lower than a later frame already accepted") != std::string::npos ||
                 doc.find("It is **not** rejected because its `sequence` is lower than a later frame already accepted") != std::string::npos);
    KRITVA_CHECK(doc.find("these relaxations apply only once a negotiated minor version actually defines them") != std::string::npos);

    // The capability ids named in section 13 are the I3 ones.
    KRITVA_CHECK(doc.find("`0x1001` acceleration, `0x1002` angular velocity, `0x1003` position, `0x2001` motor command") != std::string::npos);

    std::printf("protocol_spec_consistency_test: PASS\n");
    return 0;
}
