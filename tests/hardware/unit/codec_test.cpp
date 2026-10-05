//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : codec_test.cpp
// Description : Unit tests of the byte codec: round trips, strict validation of every field, discovery capacity, hostile lengths.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Tests
// Layer       : Hardware Abstraction
//
// Requirements: FR-002; FR-003; PR-002
// API         : CODEC-UT
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <new>
#include <string>
#include <vector>

#include "../../runtime/check.hpp"
#include "../support/codec_samples.hpp"

using namespace kritva::hardware::transport;
using namespace kritva::hardware::transport::test;
using kritva::core::ErrorCode;
using Bytes = std::vector<std::uint8_t>;

// ---- allocation tracking: the largest single allocation requested while decoding hostile input -----------
// Test-only tracking allocator: operator new and delete are replaced as a malloc/free pair (the target is built with
// -Wno-mismatched-new-delete because GCC cannot see through the pair when it inlines them).
static std::size_t g_max_allocation = 0;
void* operator new(std::size_t size) {
    g_max_allocation = std::max(g_max_allocation, size);
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

static bool bad_encode(const kritva::core::Result<Bytes>& r) { return !r.has_value() && r.error().code == ErrorCode::INVALID_ARGUMENT; }

static void test_every_sample_round_trips_canonically() {                // FR-002
    for (const Sample& s : all_samples()) {
        Bytes again;
        const auto e = decode_and_reencode(s.type, s.kind, s.payload, again);
        if (e != FrameError::NONE) std::fprintf(stderr, "sample %s failed with %d\n", s.name.c_str(), static_cast<int>(e));
        KRITVA_CHECK(e == FrameError::NONE);
        KRITVA_CHECK(again == s.payload);                                    // decode then encode gives the identical bytes
        KRITVA_CHECK(s.payload.size() <= kMaxPayloadSize);
    }
}

static void test_decoded_values() {
    {
        const auto p = encode(HelloAckPayload{{}, 9, 1, 0, 42, 100, 300}).value();
        const auto d = decode<HelloAckPayload>(p);
        KRITVA_CHECK(d.ok() && d.value.status.ok() && d.value.node_id == 9 && d.value.session_id == 42 && d.value.heartbeat_timeout_ms == 300);
    }
    {
        const auto d = decode<ConfigureRequestPayload>(encode(ConfigureRequestPayload{addr(), {{"limit", std::int64_t{-5}}, {"on", true}, {"note", std::string("hi there")}}}).value());
        KRITVA_CHECK(d.ok() && d.value.settings.size() == 3 && d.value.address.endpoint_id == 3);
        KRITVA_CHECK(std::get<std::int64_t>(d.value.settings[0].value) == -5 && std::get<bool>(d.value.settings[1].value) == true &&
                     std::get<std::string>(d.value.settings[2].value) == "hi there" && d.value.settings[0].key == "limit");
    }
    {
        const auto d = decode_read_response(encode(ReadResponsePayload{{}, ReadKind::VEC3, 1.5, -2.5, 9.81, 0, 12, -7}).value(), ReadKind::VEC3);
        KRITVA_CHECK(d.ok() && d.value.x == 1.5 && d.value.y == -2.5 && d.value.z == 9.81 && d.value.sample_sequence == 12 && d.value.timestamp_ns == -7);
        const auto s = decode_read_response(encode(ReadResponsePayload{{}, ReadKind::SCALAR, 0, 0, 0, 0.125, 1, 2}).value(), ReadKind::SCALAR);
        KRITVA_CHECK(s.ok() && s.value.value == 0.125);
    }
    {
        const auto d = decode<DiscoveryResponsePayload>(all_samples()[5].payload);       // "discovery"
        KRITVA_CHECK(d.ok() && d.value.devices.size() == 2 && d.value.devices[0].name == "left_arm_imu" && d.value.devices[1].endpoints[0].name == "command");
        KRITVA_CHECK(d.value.devices[1].endpoints[0].direction == WireDirection::ACTUATOR && d.value.devices[1].endpoints[0].capabilities[0].id == 0x2001);
    }
    {
        const auto d = decode<ObserveResponsePayload>(encode([] { ObserveResponsePayload o; o.state = kritva::core::LifecycleState::FAULT; o.health = kritva::core::HealthState::UNHEALTHY;
                                                                    o.health_detail = "x"; o.has_last_error = true; o.last_error_code = ErrorCode::TIMEOUT; o.last_error_message = "late"; return o; }()).value());
        KRITVA_CHECK(d.ok() && d.value.state == kritva::core::LifecycleState::FAULT && d.value.has_last_error && d.value.last_error_code == ErrorCode::TIMEOUT);
    }
}

static void test_encoders_reject_what_decoders_reject() {
    KRITVA_CHECK(bad_encode(encode(AddressPayload{0, 2, 3})) && bad_encode(encode(AddressPayload{1, 0, 3})) && bad_encode(encode(AddressPayload{1, 2, 0})));
    KRITVA_CHECK(bad_encode(encode(HelloPayload{0, 1, 0, 1, 1})));
    KRITVA_CHECK(bad_encode(encode(HelloAckPayload{{}, 0, 1, 0, 5, 1, 1})) && bad_encode(encode(HelloAckPayload{{}, 5, 1, 0, 0, 1, 1})));
    KRITVA_CHECK(bad_encode(encode(StatusPayload{StatusField{ErrorCode::NONE, "an OK status carries no message"}})));
    KRITVA_CHECK(bad_encode(encode(StatusPayload{StatusField{static_cast<ErrorCode>(12), "x"}})));
    KRITVA_CHECK(bad_encode(encode_protocol_error(StatusPayload{})));
    KRITVA_CHECK(bad_encode(encode(WriteRequestPayload{addr(), kNaN})) && bad_encode(encode(WriteRequestPayload{addr(), kInf})) && bad_encode(encode(WriteRequestPayload{addr(), -kInf})));
    KRITVA_CHECK(bad_encode(encode(WriteRequestPayload{{0, 1, 1}, 1.0})));
    for (double bad : {kNaN, kInf, -kInf}) {
        KRITVA_CHECK(bad_encode(encode(ReadResponsePayload{{}, ReadKind::VEC3, bad, 0, 0, 0, 1, 1})) && bad_encode(encode(ReadResponsePayload{{}, ReadKind::VEC3, 0, bad, 0, 0, 1, 1})) &&
                     bad_encode(encode(ReadResponsePayload{{}, ReadKind::VEC3, 0, 0, bad, 0, 1, 1})) && bad_encode(encode(ReadResponsePayload{{}, ReadKind::SCALAR, 0, 0, 0, bad, 1, 1})));
    }
    KRITVA_CHECK(bad_encode(encode(FaultEventPayload{addr(), kritva::core::LifecycleState::RUNNING, "x"})));
    KRITVA_CHECK(bad_encode(encode(FaultEventPayload{addr(), kritva::core::LifecycleState::FAULT, std::string("a\nb")})));
    KRITVA_CHECK(bad_encode(encode(LifecycleResponsePayload{{}, static_cast<kritva::core::LifecycleState>(8)})));
    ObserveResponsePayload o;
    o.status_code = static_cast<kritva::core::StatusCode>(10);
    KRITVA_CHECK(bad_encode(encode(o)));
    o = ObserveResponsePayload{}; o.health = static_cast<kritva::core::HealthState>(4);
    KRITVA_CHECK(bad_encode(encode(o)));
    // names and texts
    for (const char* bad : {"", "Upper", "has.dot", "has space", "dash-ed"}) KRITVA_CHECK(bad_encode(encode(ConfigureRequestPayload{addr(), {{bad, true}}})));
    KRITVA_CHECK(bad_encode(encode(ConfigureRequestPayload{addr(), {{std::string(65, 'a'), true}}})) && !bad_encode(encode(ConfigureRequestPayload{addr(), {{std::string(64, 'a'), true}}})));
    KRITVA_CHECK(bad_encode(encode(ConfigureRequestPayload{addr(), {{"k", std::string(257, 'x')}}})) && !bad_encode(encode(ConfigureRequestPayload{addr(), {{"k", std::string(256, 'x')}}})));
    ConfigureRequestPayload many{addr(), {}};
    for (int i = 0; i < 17; ++i) many.settings.push_back({"k", true});
    KRITVA_CHECK(bad_encode(encode(many)));
    // discovery
    KRITVA_CHECK(bad_encode(encode(big_discovery(65, 0))));
    KRITVA_CHECK(bad_encode(encode(big_discovery(1, 257))) && bad_encode(encode(big_discovery(64, 5))));            // 257 and 320 endpoints
    auto dup_device = big_discovery(2, 1); dup_device.devices[1].id = dup_device.devices[0].id;
    auto dup_device_name = big_discovery(2, 1); dup_device_name.devices[1].name = dup_device_name.devices[0].name;
    auto dup_endpoint = big_discovery(1, 2); dup_endpoint.devices[0].endpoints[1].id = dup_endpoint.devices[0].endpoints[0].id;
    auto dup_endpoint_name = big_discovery(1, 2); dup_endpoint_name.devices[0].endpoints[1].name = dup_endpoint_name.devices[0].endpoints[0].name;
    auto no_caps = big_discovery(1, 1); no_caps.devices[0].endpoints[0].capabilities.clear();
    auto too_many_caps = big_discovery(1, 1); too_many_caps.devices[0].endpoints[0].capabilities.assign(17, {1, "c"});
    auto bad_direction = big_discovery(1, 1); bad_direction.devices[0].endpoints[0].direction = static_cast<WireDirection>(2);
    auto zero_id = big_discovery(1, 1); zero_id.devices[0].id = 0;
    for (const auto& d : {dup_device, dup_device_name, dup_endpoint, dup_endpoint_name, no_caps, too_many_caps, bad_direction, zero_id}) KRITVA_CHECK(bad_encode(encode(d)));
    // The same endpoint ids on different devices are legal (ids are unique only within a device).
    KRITVA_CHECK(encode(big_discovery(2, 2)).has_value());
}

// ---- hand-built hostile payloads ---------------------------------------------------------------------------

static void put_f64_bits(ByteWriter& w, std::uint64_t bits) { w.u64(bits); }

static void test_non_finite_numbers_are_rejected_on_decode() {
    const std::uint64_t nan_bits = 0x7FF8000000000000ull, inf_bits = 0x7FF0000000000000ull, ninf_bits = 0xFFF0000000000000ull, snan_bits = 0x7FF0000000000001ull;
    for (std::uint64_t bits : {nan_bits, inf_bits, ninf_bits, snan_bits, std::uint64_t{0xFFFFFFFFFFFFFFFFull}}) {
        {   ByteWriter w; w.u64(1); w.u64(2); w.u64(3); put_f64_bits(w, bits);
            KRITVA_CHECK(decode<WriteRequestPayload>(w.take()).error == FrameError::BAD_PAYLOAD); }
        for (int field = 0; field < 3; ++field) {
            ByteWriter w; w.u16(0);
            for (int i = 0; i < 3; ++i) { if (i == field) put_f64_bits(w, bits); else w.f64(0.0); }
            w.u64(1); w.i64(1);
            KRITVA_CHECK(decode_read_response(w.take(), ReadKind::VEC3).error == FrameError::BAD_PAYLOAD);
        }
        {   ByteWriter w; w.u16(0); put_f64_bits(w, bits); w.u64(1); w.i64(1);
            KRITVA_CHECK(decode_read_response(w.take(), ReadKind::SCALAR).error == FrameError::BAD_PAYLOAD); }
    }
    {   ByteWriter w; w.u64(1); w.u64(2); w.u64(3); w.f64(-0.0);                     // negative zero and subnormals are finite
        KRITVA_CHECK(decode<WriteRequestPayload>(w.take()).ok()); }
    {   ByteWriter w; w.u64(1); w.u64(2); w.u64(3); put_f64_bits(w, 1); KRITVA_CHECK(decode<WriteRequestPayload>(w.take()).ok()); }
}

static Bytes text_payload(const Bytes& raw) {                                      // a PROTOCOL_ERROR carrying `raw` as its message
    ByteWriter w;
    w.u16(2);                                                                      // INVALID_ARGUMENT
    w.string(std::string_view(reinterpret_cast<const char*>(raw.data()), raw.size()));
    return w.take();
}

static void test_text_is_strict_utf8_without_control_characters() {
    const auto ok = [](const Bytes& raw) { return decode_protocol_error(text_payload(raw)).ok(); };
    KRITVA_CHECK(ok({'o', 'k'}) && ok({}) && ok({0xC3, 0xA9}) && ok({0xE2, 0x82, 0xAC}) && ok({0xF0, 0x9F, 0x99, 0x82}));   // ASCII, 2, 3, 4 byte forms
    KRITVA_CHECK(ok({0xC2, 0x85}));                                                // U+0085 is not an ASCII control character: allowed by the spec
    KRITVA_CHECK(ok({0xF4, 0x8F, 0xBF, 0xBF}) && ok({0xED, 0x9F, 0xBF}) && ok({0xEE, 0x80, 0x80}));                           // U+10FFFF, U+D7FF, U+E000
    const std::vector<Bytes> bad{
        {0x00}, {0x1F}, {0x7F}, {'a', '\n'}, {'\t'}, {'\r'}, {0x1B},               // control characters
        {0xC0, 0x80}, {0xC1, 0xBF}, {0xE0, 0x80, 0x80}, {0xE0, 0x9F, 0xBF}, {0xF0, 0x80, 0x80, 0x80}, {0xF0, 0x8F, 0xBF, 0xBF},   // overlong forms
        {0xED, 0xA0, 0x80}, {0xED, 0xBF, 0xBF},                                    // surrogates
        {0xF4, 0x90, 0x80, 0x80}, {0xF5, 0x80, 0x80, 0x80}, {0xF8, 0x88, 0x80, 0x80, 0x80}, {0xFF}, {0xFE},   // above U+10FFFF, invalid lead bytes
        {0x80}, {0xBF}, {0xC2}, {0xE2, 0x82}, {0xF0, 0x9F, 0x99}, {0xC2, 0x41}, {0xE2, 0x82, 0x41}, {0xC3, 0xC3}};   // lone continuation, truncated, bad continuation
    for (const auto& raw : bad) KRITVA_CHECK(!ok(raw));
    KRITVA_CHECK(ok(Bytes(256, 'a')) && !ok(Bytes(257, 'a')));                     // length limit
    KRITVA_CHECK(is_valid_wire_text("plain") && !is_valid_wire_text("tab\t") && !is_valid_wire_text(std::string(257, 'a')));
}

static void test_names_and_lengths_on_decode() {
    const auto cfg = [](const std::string& key) {
        ByteWriter w; w.u64(1); w.u64(2); w.u64(3); w.u16(1); w.string(key); w.u8(0); w.u8(1);
        return decode<ConfigureRequestPayload>(w.take());
    };
    KRITVA_CHECK(cfg("limit").ok() && cfg("a").ok() && cfg(std::string(64, 'z')).ok() && cfg("k_9").ok());
    for (const char* bad : {"", "Limit", "a.b", "a b", "a-b", "\xC3\xA9"}) KRITVA_CHECK(!cfg(bad).ok());
    KRITVA_CHECK(!cfg(std::string(65, 'a')).ok());
    KRITVA_CHECK(is_valid_wire_name("abc_123") && !is_valid_wire_name("") && !is_valid_wire_name("ABC") && !is_valid_wire_name(std::string(65, 'a')));
}

static void test_enumerations_status_and_ids_on_decode() {
    const auto observe = [](std::uint8_t state, std::uint8_t code, std::uint8_t health, std::uint8_t has, std::uint16_t last_code) {
        ByteWriter w; w.u16(0); w.u8(state); w.u8(code); w.u8(health); w.string(""); w.u64(0); w.u64(0); w.u8(has);
        if (has == 1) { w.u16(last_code); w.string(""); }
        return decode<ObserveResponsePayload>(w.take());
    };
    KRITVA_CHECK(observe(7, 9, 3, 0, 0).ok() && observe(0, 0, 0, 1, 11).ok());
    KRITVA_CHECK(!observe(8, 0, 0, 0, 0).ok() && !observe(0, 10, 0, 0, 0).ok() && !observe(0, 0, 4, 0, 0).ok() && !observe(0, 0, 0, 2, 0).ok() && !observe(0, 0, 0, 1, 12).ok());
    for (std::uint16_t status : {std::uint16_t{12}, std::uint16_t{100}, std::uint16_t{0xFFFF}}) {
        ByteWriter w; w.u16(status); w.string("x");
        KRITVA_CHECK(decode<StatusPayload>(w.take()).error == FrameError::BAD_PAYLOAD);
    }
    {   ByteWriter w; w.u16(3);                                                    // a failing status without its message
        KRITVA_CHECK(decode<StatusPayload>(w.take()).error == FrameError::BAD_PAYLOAD); }
    {   ByteWriter w; w.u16(0); w.string("an OK status cannot carry a message");   // OK followed by anything is trailing data
        KRITVA_CHECK(decode<StatusPayload>(w.take()).error == FrameError::BAD_PAYLOAD); }
    {   ByteWriter w; w.u16(0);
        KRITVA_CHECK(decode<StatusPayload>(w.take()).ok() && !decode_protocol_error(Bytes{0, 0}).ok()); }                       // PROTOCOL_ERROR is never OK
    for (const auto& ids : {std::vector<std::uint64_t>{0, 2, 3}, {1, 0, 3}, {1, 2, 0}}) {
        ByteWriter w; for (auto v : ids) w.u64(v);
        KRITVA_CHECK(decode<AddressPayload>(w.take()).error == FrameError::BAD_PAYLOAD);
    }
    {   ByteWriter w; w.u64(0); w.u16(1); w.u16(0); w.u32(1); w.u32(1);
        KRITVA_CHECK(decode<HelloPayload>(w.take()).error == FrameError::BAD_PAYLOAD); }                                        // node id 0
    {   ByteWriter w; w.u16(0); w.u64(1); w.u16(1); w.u16(0); w.u64(0); w.u32(1); w.u32(1);
        KRITVA_CHECK(decode<HelloAckPayload>(w.take()).error == FrameError::BAD_PAYLOAD); }                                     // session id 0 in an OK ack
    for (std::uint8_t state : {std::uint8_t{0}, std::uint8_t{5}, std::uint8_t{7}, std::uint8_t{255}}) {
        ByteWriter w; w.u64(1); w.u64(2); w.u64(3); w.u8(state); w.string("x");
        KRITVA_CHECK(decode<FaultEventPayload>(w.take()).error == FrameError::BAD_PAYLOAD);                                      // only FAULT (6)
    }
    {   ByteWriter w; w.u64(1); w.u64(2); w.u64(3); w.u8(6); w.string("x");
        KRITVA_CHECK(decode<FaultEventPayload>(w.take()).ok()); }
    const auto setting = [](std::uint8_t type, std::uint8_t value) { ByteWriter w; w.u64(1); w.u64(2); w.u64(3); w.u16(1); w.string("k"); w.u8(type); w.u8(value);
                                                                      return decode<ConfigureRequestPayload>(w.take()); };
    KRITVA_CHECK(setting(0, 0).ok() && setting(0, 1).ok() && !setting(0, 2).ok() && !setting(3, 0).ok() && !setting(255, 0).ok());
    {   ByteWriter w; w.u16(0); w.u8(9);                                                                                           // lifecycle state 9
        KRITVA_CHECK(decode<LifecycleResponsePayload>(w.take()).error == FrameError::BAD_PAYLOAD); }
}

static Bytes discovery_bytes(std::uint16_t devices, auto&& body) {
    ByteWriter w; w.u16(0); w.u16(devices); body(w);
    return w.take();
}

static void test_discovery_structure_on_decode() {
    const auto one_endpoint = [](ByteWriter& w, std::uint64_t dev, const char* dname, std::uint64_t ep, const char* ename, std::uint8_t dir, std::uint16_t caps) {
        w.u64(dev); w.string(dname); w.u16(1); w.u64(ep); w.string(ename); w.u8(dir); w.u16(caps);
        for (std::uint16_t i = 0; i < caps; ++i) { w.u64(0x1001 + i); w.string("cap"); }
    };
    KRITVA_CHECK(decode<DiscoveryResponsePayload>(discovery_bytes(1, [&](ByteWriter& w) { one_endpoint(w, 1, "d", 1, "e", 0, 1); })).ok());
    KRITVA_CHECK(decode<DiscoveryResponsePayload>(discovery_bytes(1, [&](ByteWriter& w) { one_endpoint(w, 1, "d", 1, "e", 1, 16); })).ok());           // 16 capabilities on the wire
    KRITVA_CHECK(!decode<DiscoveryResponsePayload>(discovery_bytes(1, [&](ByteWriter& w) { one_endpoint(w, 1, "d", 1, "e", 2, 1); })).ok());         // direction 2
    KRITVA_CHECK(!decode<DiscoveryResponsePayload>(discovery_bytes(1, [&](ByteWriter& w) { one_endpoint(w, 1, "d", 1, "e", 0, 0); })).ok());         // 0 capabilities
    KRITVA_CHECK(!decode<DiscoveryResponsePayload>(discovery_bytes(1, [&](ByteWriter& w) { one_endpoint(w, 1, "d", 1, "e", 0, 17); })).ok());        // 17 capabilities
    KRITVA_CHECK(!decode<DiscoveryResponsePayload>(discovery_bytes(1, [&](ByteWriter& w) { one_endpoint(w, 0, "d", 1, "e", 0, 1); })).ok());         // device id 0
    KRITVA_CHECK(!decode<DiscoveryResponsePayload>(discovery_bytes(1, [&](ByteWriter& w) { one_endpoint(w, 1, "D", 1, "e", 0, 1); })).ok());         // bad name
    KRITVA_CHECK(!decode<DiscoveryResponsePayload>(discovery_bytes(1, [&](ByteWriter& w) { one_endpoint(w, 1, "d", 0, "e", 0, 1); })).ok());         // endpoint id 0
    KRITVA_CHECK(!decode<DiscoveryResponsePayload>(discovery_bytes(2, [&](ByteWriter& w) { one_endpoint(w, 1, "a", 1, "e", 0, 1); one_endpoint(w, 1, "b", 1, "e", 0, 1); })).ok());   // duplicate device id
    KRITVA_CHECK(!decode<DiscoveryResponsePayload>(discovery_bytes(2, [&](ByteWriter& w) { one_endpoint(w, 1, "a", 1, "e", 0, 1); one_endpoint(w, 2, "a", 1, "e", 0, 1); })).ok());   // duplicate device name
    KRITVA_CHECK(decode<DiscoveryResponsePayload>(discovery_bytes(2, [&](ByteWriter& w) { one_endpoint(w, 1, "a", 1, "e", 0, 1); one_endpoint(w, 2, "b", 1, "e", 0, 1); })).ok());    // same endpoint id and name on two devices: legal
    KRITVA_CHECK(!decode<DiscoveryResponsePayload>(discovery_bytes(1, [&](ByteWriter& w) {
        w.u64(1); w.string("d"); w.u16(2);
        for (int i = 0; i < 2; ++i) { w.u64(1); w.string(i ? "f" : "e"); w.u8(0); w.u16(1); w.u64(0x1001); w.string("c"); } })).ok());                           // duplicate endpoint id in one device
    KRITVA_CHECK(!decode<DiscoveryResponsePayload>(discovery_bytes(1, [&](ByteWriter& w) {
        w.u64(1); w.string("d"); w.u16(2);
        for (int i = 0; i < 2; ++i) { w.u64(i + 1); w.string("e"); w.u8(0); w.u16(1); w.u64(0x1001); w.string("c"); } })).ok());                                    // duplicate endpoint name
}

static void test_discovery_capacity_and_the_frame_bound() {              // spec section 13
    const auto biggest = big_discovery(64, 4);
    const auto size = encoded_size(biggest);
    KRITVA_CHECK(size.has_value() && size.value() == kMaxDiscoveryPayload && size.value() == 43524);
    const auto bytes = encode(biggest);
    KRITVA_CHECK(bytes.has_value() && bytes.value().size() == size.value() && bytes.value().size() <= kMaxPayloadSize);
    const auto back = decode<DiscoveryResponsePayload>(bytes.value());
    KRITVA_CHECK(back.ok() && back.value.devices.size() == 64);
    // 256 endpoints with 16 capabilities each exceed one frame on the wire: the size is computed first and the encoder refuses before output.
    DiscoveryResponsePayload fat = big_discovery(64, 4);
    for (auto& d : fat.devices) for (auto& e : d.endpoints) e.capabilities.assign(16, {0x1001, std::string(64, 'c')});
    const auto fat_size = encoded_size(fat);
    KRITVA_CHECK(fat_size.has_value() && fat_size.value() > kMaxPayloadSize);
    const auto refused = encode(fat);
    KRITVA_CHECK(!refused.has_value() && refused.error().message == "discovery does not fit one frame");
    // A discovery response is also a frame: the biggest legal one fits one frame with room to spare.
    KRITVA_CHECK(encode_frame(FrameHeader{}, bytes.value()).has_value());
}

static void test_hostile_lengths_do_not_drive_allocation() {            // untrusted length -> range check -> only then allocation
    g_max_allocation = 0;
    const auto rejected = [](const Bytes& b) { return decode<DiscoveryResponsePayload>(b).error == FrameError::BAD_PAYLOAD; };
    KRITVA_CHECK(rejected(discovery_bytes(64, [](ByteWriter&) {})));                                         // claims 64 devices, carries none
    KRITVA_CHECK(rejected(discovery_bytes(65535, [](ByteWriter&) {})));
    KRITVA_CHECK(rejected(discovery_bytes(1, [](ByteWriter& w) { w.u64(1); w.string("d"); w.u16(65535); })));        // claims 65535 endpoints
    KRITVA_CHECK(rejected(discovery_bytes(1, [](ByteWriter& w) { w.u64(1); w.string("d"); w.u16(256); })));          // claims 256, carries none
    KRITVA_CHECK(rejected(discovery_bytes(1, [](ByteWriter& w) { w.u64(1); w.string("d"); w.u16(1); w.u64(1); w.string("e"); w.u8(0); w.u16(65535); })));   // 65535 capabilities
    Bytes long_string{2, 0, 0xFF, 0xFF};                                                                       // a status message claiming 65535 bytes
    KRITVA_CHECK(decode<StatusPayload>(long_string).error == FrameError::BAD_PAYLOAD);
    ByteWriter cfg; cfg.u64(1); cfg.u64(2); cfg.u64(3); cfg.u16(65535);                                      // claims 65535 settings
    KRITVA_CHECK(decode<ConfigureRequestPayload>(cfg.take()).error == FrameError::BAD_PAYLOAD);
    ByteWriter cfg2; cfg2.u64(1); cfg2.u64(2); cfg2.u64(3); cfg2.u16(16);                                    // 16 settings, no bytes for them
    KRITVA_CHECK(decode<ConfigureRequestPayload>(cfg2.take()).error == FrameError::BAD_PAYLOAD);
    KRITVA_CHECK(g_max_allocation <= 1024);                                                                  // nothing large was requested on the claim of a length
}

static void test_writer_and_reader_bounds() {
    ByteWriter w(4);                                                                                         // a writer cannot exceed its limit
    w.u32(1);
    KRITVA_CHECK(w.ok() && w.size() == 4);
    w.u8(1);
    KRITVA_CHECK(!w.ok() && w.size() == 4);
    ByteWriter f; f.f64(kNaN);
    KRITVA_CHECK(!f.ok() && f.size() == 0);                                                                  // nothing non-finite is written
    ByteWriter s; s.string(std::string(70000, 'x'));
    KRITVA_CHECK(!s.ok());
    const Bytes three{1, 2, 3};
    ByteReader r(three);
    KRITVA_CHECK(r.u16() == 0x0201 && r.remaining() == 1 && !r.failed());
    KRITVA_CHECK(r.u32() == 0 && r.failed() && r.remaining() == 1);                                           // a short read fails and consumes nothing
    KRITVA_CHECK(r.u8() == 0 && r.failed());                                                                  // every later read returns zero
    ByteReader empty(ByteSpan{});
    KRITVA_CHECK(empty.u8() == 0 && empty.failed());
}

int main() {
    test_every_sample_round_trips_canonically();
    test_decoded_values();
    test_encoders_reject_what_decoders_reject();
    test_non_finite_numbers_are_rejected_on_decode();
    test_text_is_strict_utf8_without_control_characters();
    test_names_and_lengths_on_decode();
    test_enumerations_status_and_ids_on_decode();
    test_discovery_structure_on_decode();
    test_discovery_capacity_and_the_frame_bound();
    test_hostile_lengths_do_not_drive_allocation();
    test_writer_and_reader_bounds();
    std::printf("codec_test: PASS\n");
    return 0;
}
