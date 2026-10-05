//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : codec.cpp
// Description : Byte reader and writer, string validators and the payload encoders and decoders.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Transport Codec
// Layer       : Hardware Abstraction
//
// Requirements: FR-002; FR-003; PR-002
// API         : kritva::hardware::transport::encode / decode
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#include <kritva/hardware/transport/codec.hpp>

#include <bit>
#include <cmath>
#include <set>

namespace kritva::hardware::transport {

using core::ErrorCode;

// The enumerations are validated by range on the wire; keep the ranges tied to the released Core enums.
static_assert(static_cast<unsigned>(core::LifecycleState::RECOVERING) == 7, "LifecycleState range changed");
static_assert(static_cast<unsigned>(core::StatusCode::INTERNAL_ERROR) == 9, "StatusCode range changed");
static_assert(static_cast<unsigned>(core::HealthState::UNHEALTHY) == 3, "HealthState range changed");

constexpr std::uint8_t kMaxLifecycle = 7;
constexpr std::uint8_t kMaxStatusCode = 9;
constexpr std::uint8_t kMaxHealth = 3;

// ---- validators -------------------------------------------------------------------------------------

bool is_valid_wire_name(std::string_view s) noexcept {
    if (s.empty() || s.size() > kMaxNameLength) return false;
    for (char c : s) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

bool is_valid_wire_text(std::string_view s) noexcept {
    if (s.size() > kMaxTextLength) return false;
    const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(s[i]); };
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char b = byte(i);
        if (b < 0x80) {                                           // ASCII: no control characters
            if (b < 0x20 || b == 0x7F) return false;
            ++i;
            continue;
        }
        std::size_t length = 0;
        unsigned char low = 0x80, high = 0xBF;                    // allowed range of the first continuation byte
        if (b >= 0xC2 && b <= 0xDF) length = 2;                   // C0 and C1 would be overlong
        else if (b == 0xE0) { length = 3; low = 0xA0; }           // no overlong 3-byte forms
        else if (b == 0xED) { length = 3; high = 0x9F; }          // no surrogates
        else if (b >= 0xE1 && b <= 0xEF) length = 3;
        else if (b == 0xF0) { length = 4; low = 0x90; }           // no overlong 4-byte forms
        else if (b >= 0xF1 && b <= 0xF3) length = 4;
        else if (b == 0xF4) { length = 4; high = 0x8F; }          // nothing above U+10FFFF
        else return false;
        if (i + length > s.size()) return false;
        if (byte(i + 1) < low || byte(i + 1) > high) return false;
        for (std::size_t k = 2; k < length; ++k) {
            if (byte(i + k) < 0x80 || byte(i + k) > 0xBF) return false;
        }
        i += length;
    }
    return true;
}

// ---- writer -----------------------------------------------------------------------------------------

void ByteWriter::put(std::uint64_t value, std::size_t size) {
    if (!ok_ || buffer_.size() + size > limit_) { ok_ = false; return; }
    for (std::size_t i = 0; i < size; ++i) buffer_.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

void ByteWriter::f64(double v) {
    if (!std::isfinite(v)) { ok_ = false; return; }
    put(std::bit_cast<std::uint64_t>(v), 8);
}

void ByteWriter::string(std::string_view s) {
    if (s.size() > 0xFFFF || !ok_ || buffer_.size() + 2 + s.size() > limit_) { ok_ = false; return; }
    put(s.size(), 2);
    for (char c : s) buffer_.push_back(static_cast<std::uint8_t>(c));
}

// ---- reader -----------------------------------------------------------------------------------------

std::uint64_t ByteReader::take(std::size_t size) {
    if (failed_ || remaining() < size) { failed_ = true; return 0; }
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < size; ++i) v |= static_cast<std::uint64_t>(bytes_[position_ + i]) << (8 * i);
    position_ += size;
    return v;
}

double ByteReader::f64() {
    const std::uint64_t raw = take(8);
    if (failed_) return 0.0;
    const double d = std::bit_cast<double>(raw);
    if (!std::isfinite(d)) { failed_ = true; return 0.0; }
    return d;
}

std::string ByteReader::string(std::size_t min, std::size_t max, bool is_name) {
    const std::size_t length = u16();
    // The untrusted length is checked against the limit and against the bytes that remain BEFORE anything is allocated.
    if (failed_ || length < min || length > max || length > remaining()) { failed_ = true; return {}; }
    const std::string_view view(reinterpret_cast<const char*>(bytes_.data() + position_), length);
    if (!(is_name ? is_valid_wire_name(view) : is_valid_wire_text(view))) { failed_ = true; return {}; }
    position_ += length;
    return std::string(view);
}

std::string ByteReader::name() { return string(1, kMaxNameLength, true); }
std::string ByteReader::text() { return string(0, kMaxTextLength, false); }

// ---- shared field helpers ------------------------------------------------------------------------------

namespace {

using Bytes = std::vector<std::uint8_t>;
using Encoded = core::Result<Bytes>;

Encoded bad_value(const char* why) {
    return Encoded::failure(core::Error{ErrorCode::INVALID_ARGUMENT, core::ErrorSeverity::ERROR, {}, {}, why});
}

template <class T>
Decoded<T> bad_payload() { Decoded<T> d; d.error = FrameError::BAD_PAYLOAD; return d; }

// Finishes a decode: any failure, trailing byte or missing byte is BAD_PAYLOAD (protocol 1.0: exact consumption).
template <class T>
Decoded<T> finish(const ByteReader& r, T&& value) {
    if (r.failed() || r.remaining() != 0) return bad_payload<T>();
    Decoded<T> d;
    d.value = std::move(value);
    return d;
}

void read_status(ByteReader& r, StatusField& s) {
    const auto code = from_wire_status(r.u16());
    if (r.failed() || !code.has_value()) { r.fail(); return; }
    s.code = *code;
    if (!s.ok()) s.message = r.text();
}

void write_status(ByteWriter& w, const StatusField& s) {
    w.u16(to_wire_status(s.code));
    if (!s.ok()) w.string(s.message);
}

[[nodiscard]] bool valid_status(const StatusField& s) {
    if (static_cast<std::uint32_t>(s.code) > kMaxWireStatus) return false;
    return s.ok() ? s.message.empty() : is_valid_wire_text(s.message);
}

AddressPayload read_address(ByteReader& r) {
    AddressPayload a;
    a.node_id = r.u64();
    a.device_id = r.u64();
    a.endpoint_id = r.u64();
    if (a.node_id == 0 || a.device_id == 0 || a.endpoint_id == 0) r.fail();       // every id is non-zero
    return a;
}

void write_address(ByteWriter& w, const AddressPayload& a) { w.u64(a.node_id); w.u64(a.device_id); w.u64(a.endpoint_id); }
[[nodiscard]] bool valid_address(const AddressPayload& a) { return a.node_id != 0 && a.device_id != 0 && a.endpoint_id != 0; }

Encoded finish_encode(ByteWriter& w) {
    if (!w.ok()) return bad_value("the payload does not fit or contains a non-finite number");
    return Encoded::success(w.take());
}

} // namespace

// ---- encoders ---------------------------------------------------------------------------------------------

Encoded encode(const AddressPayload& p) {
    if (!valid_address(p)) return bad_value("address ids must be non-zero");
    ByteWriter w;
    write_address(w, p);
    return finish_encode(w);
}

Encoded encode(const HelloPayload& p) {
    if (p.node_id == 0) return bad_value("node id must be non-zero");
    ByteWriter w;
    w.u64(p.node_id); w.u16(p.major); w.u16(p.minor); w.u32(p.heartbeat_period_ms); w.u32(p.heartbeat_timeout_ms);
    return finish_encode(w);
}

Encoded encode(const HelloAckPayload& p) {
    if (!valid_status(p.status)) return bad_value("invalid status");
    ByteWriter w;
    write_status(w, p.status);
    if (p.status.ok()) {
        if (p.node_id == 0 || p.session_id == 0) return bad_value("node id and session id must be non-zero");
        w.u64(p.node_id); w.u16(p.major); w.u16(p.minor); w.u64(p.session_id);
        w.u32(p.heartbeat_period_ms); w.u32(p.heartbeat_timeout_ms);
    }
    return finish_encode(w);
}

core::Result<std::size_t> encoded_size(const DiscoveryResponsePayload& p) {
    using R = core::Result<std::size_t>;
    const auto bad = [](const char* why) { return R::failure(core::Error{ErrorCode::INVALID_ARGUMENT, core::ErrorSeverity::ERROR, {}, {}, why}); };
    if (!valid_status(p.status)) return bad("invalid status");
    std::size_t size = 2 + (p.status.ok() ? 0 : 2 + p.status.message.size());
    if (!p.status.ok()) return R::success(size);
    if (p.devices.size() > kMaxDevices) return bad("too many devices");
    size += 2;
    std::size_t endpoints = 0;
    std::set<std::uint64_t> device_ids;
    std::set<std::string> device_names;
    for (const auto& d : p.devices) {
        if (d.id == 0 || !is_valid_wire_name(d.name)) return bad("invalid device");
        if (!device_ids.insert(d.id).second || !device_names.insert(d.name).second) return bad("duplicate device id or name");
        if (d.endpoints.size() > kMaxDiscoveryItems) return bad("too many endpoints");
        endpoints += d.endpoints.size();
        if (endpoints > kMaxDiscoveryItems) return bad("too many endpoints");
        size += 8 + 2 + d.name.size() + 2;
        std::set<std::uint64_t> endpoint_ids;
        std::set<std::string> endpoint_names;
        for (const auto& e : d.endpoints) {
            if (e.id == 0 || !is_valid_wire_name(e.name)) return bad("invalid endpoint");
            if (static_cast<std::uint8_t>(e.direction) > 1) return bad("invalid direction");
            if (!endpoint_ids.insert(e.id).second || !endpoint_names.insert(e.name).second) return bad("duplicate endpoint id or name");
            if (e.capabilities.empty() || e.capabilities.size() > kMaxCapabilitiesPerEndpoint) return bad("invalid capability count");
            size += 8 + 2 + e.name.size() + 1 + 2;
            for (const auto& c : e.capabilities) {
                if (c.id == 0 || !is_valid_wire_name(c.name)) return bad("invalid capability");
                size += 8 + 2 + c.name.size();
            }
        }
    }
    return R::success(size);
}

Encoded encode(const DiscoveryResponsePayload& p) {
    const auto size = encoded_size(p);                                       // validates, and computes the size before encoding
    if (!size) return Encoded::failure(size.error());
    if (size.value() > kMaxPayloadSize) return bad_value("discovery does not fit one frame");
    ByteWriter w;
    write_status(w, p.status);
    if (p.status.ok()) {
        w.u16(static_cast<std::uint16_t>(p.devices.size()));
        for (const auto& d : p.devices) {
            w.u64(d.id); w.string(d.name); w.u16(static_cast<std::uint16_t>(d.endpoints.size()));
            for (const auto& e : d.endpoints) {
                w.u64(e.id); w.string(e.name); w.u8(static_cast<std::uint8_t>(e.direction));
                w.u16(static_cast<std::uint16_t>(e.capabilities.size()));
                for (const auto& c : e.capabilities) { w.u64(c.id); w.string(c.name); }
            }
        }
    }
    auto out = finish_encode(w);
    if (out && out.value().size() != size.value()) return bad_value("internal size mismatch");
    return out;
}

Encoded encode(const ConfigureRequestPayload& p) {
    if (!valid_address(p.address)) return bad_value("address ids must be non-zero");
    if (p.settings.size() > kMaxConfigureSettings) return bad_value("too many settings");
    ByteWriter w;
    write_address(w, p.address);
    w.u16(static_cast<std::uint16_t>(p.settings.size()));
    for (const auto& s : p.settings) {
        if (!is_valid_wire_name(s.key)) return bad_value("invalid setting key");
        w.string(s.key);
        w.u8(static_cast<std::uint8_t>(s.value.index()));
        if (const bool* b = std::get_if<bool>(&s.value)) w.u8(*b ? 1 : 0);
        else if (const auto* i = std::get_if<std::int64_t>(&s.value)) w.i64(*i);
        else {
            const auto& text = std::get<std::string>(s.value);
            if (!is_valid_wire_text(text)) return bad_value("invalid setting text");
            w.string(text);
        }
    }
    return finish_encode(w);
}

Encoded encode(const LifecycleResponsePayload& p) {
    if (!valid_status(p.status)) return bad_value("invalid status");
    if (static_cast<unsigned>(p.state) > kMaxLifecycle) return bad_value("invalid lifecycle state");
    ByteWriter w;
    write_status(w, p.status);
    if (p.status.ok()) w.u8(static_cast<std::uint8_t>(p.state));
    return finish_encode(w);
}

Encoded encode(const ReadResponsePayload& p) {
    if (!valid_status(p.status)) return bad_value("invalid status");
    ByteWriter w;
    write_status(w, p.status);
    if (p.status.ok()) {
        if (p.kind == ReadKind::VEC3) { w.f64(p.x); w.f64(p.y); w.f64(p.z); } else { w.f64(p.value); }
        w.u64(p.sample_sequence);
        w.i64(p.timestamp_ns);
    }
    return finish_encode(w);
}

Encoded encode(const WriteRequestPayload& p) {
    if (!valid_address(p.address)) return bad_value("address ids must be non-zero");
    ByteWriter w;
    write_address(w, p.address);
    w.f64(p.velocity_rad_s);
    return finish_encode(w);
}

Encoded encode(const StatusPayload& p) {
    if (!valid_status(p.status)) return bad_value("invalid status");
    ByteWriter w;
    write_status(w, p.status);
    return finish_encode(w);
}

Encoded encode_protocol_error(const StatusPayload& p) {
    if (p.status.ok()) return bad_value("a PROTOCOL_ERROR status is never OK");
    return encode(p);
}

Encoded encode(const ObserveResponsePayload& p) {
    if (!valid_status(p.status)) return bad_value("invalid status");
    ByteWriter w;
    write_status(w, p.status);
    if (p.status.ok()) {
        if (static_cast<unsigned>(p.state) > kMaxLifecycle || static_cast<unsigned>(p.status_code) > kMaxStatusCode ||
            static_cast<unsigned>(p.health) > kMaxHealth) return bad_value("invalid enumeration value");
        if (!is_valid_wire_text(p.health_detail)) return bad_value("invalid health detail");
        w.u8(static_cast<std::uint8_t>(p.state)); w.u8(static_cast<std::uint8_t>(p.status_code)); w.u8(static_cast<std::uint8_t>(p.health));
        w.string(p.health_detail);
        w.u64(p.operations_ok); w.u64(p.operations_failed);
        w.u8(p.has_last_error ? 1 : 0);
        if (p.has_last_error) {
            if (static_cast<std::uint32_t>(p.last_error_code) > kMaxWireStatus || !is_valid_wire_text(p.last_error_message)) {
                return bad_value("invalid last error");
            }
            w.u16(to_wire_status(p.last_error_code));
            w.string(p.last_error_message);
        }
    }
    return finish_encode(w);
}

Encoded encode(const FaultEventPayload& p) {
    if (!valid_address(p.address)) return bad_value("address ids must be non-zero");
    if (p.state != core::LifecycleState::FAULT) return bad_value("a FAULT_EVENT reports the FAULT state");
    if (!is_valid_wire_text(p.reason)) return bad_value("invalid reason");
    ByteWriter w;
    write_address(w, p.address);
    w.u8(static_cast<std::uint8_t>(p.state));
    w.string(p.reason);
    return finish_encode(w);
}

Encoded encode(const HeartbeatPayload& p) {
    ByteWriter w;
    w.u64(p.sender_time_ns);
    return finish_encode(w);
}

// ---- decoders ---------------------------------------------------------------------------------------------

FrameError decode_empty(ByteSpan payload) noexcept {
    return payload.empty() ? FrameError::NONE : FrameError::BAD_PAYLOAD;
}

template <> Decoded<AddressPayload> decode<AddressPayload>(ByteSpan payload) {
    ByteReader r(payload);
    AddressPayload a = read_address(r);
    return finish(r, std::move(a));
}

template <> Decoded<HelloPayload> decode<HelloPayload>(ByteSpan payload) {
    ByteReader r(payload);
    HelloPayload p;
    p.node_id = r.u64(); p.major = r.u16(); p.minor = r.u16(); p.heartbeat_period_ms = r.u32(); p.heartbeat_timeout_ms = r.u32();
    if (p.node_id == 0) r.fail();
    return finish(r, std::move(p));
}

template <> Decoded<HelloAckPayload> decode<HelloAckPayload>(ByteSpan payload) {
    ByteReader r(payload);
    HelloAckPayload p;
    read_status(r, p.status);
    if (p.status.ok()) {
        p.node_id = r.u64(); p.major = r.u16(); p.minor = r.u16(); p.session_id = r.u64();
        p.heartbeat_period_ms = r.u32(); p.heartbeat_timeout_ms = r.u32();
        if (p.node_id == 0 || p.session_id == 0) r.fail();
    }
    return finish(r, std::move(p));
}

template <> Decoded<DiscoveryResponsePayload> decode<DiscoveryResponsePayload>(ByteSpan payload) {
    // Smallest possible encodings, used to reject a count that the remaining bytes cannot hold, before iterating.
    constexpr std::size_t kMinDevice = 8 + 2 + 1 + 2;
    constexpr std::size_t kMinCapability = 8 + 2 + 1;
    constexpr std::size_t kMinEndpoint = 8 + 2 + 1 + 1 + 2 + kMinCapability;
    ByteReader r(payload);
    DiscoveryResponsePayload p;
    read_status(r, p.status);
    if (p.status.ok()) {
        const std::size_t device_count = r.u16();
        if (r.failed() || device_count > kMaxDevices || device_count * kMinDevice > r.remaining()) r.fail();
        std::size_t endpoints_total = 0;
        std::set<std::uint64_t> device_ids;
        std::set<std::string> device_names;
        for (std::size_t i = 0; i < device_count && !r.failed(); ++i) {
            DiscoveredDevice d;
            d.id = r.u64();
            d.name = r.name();
            const std::size_t endpoint_count = r.u16();
            endpoints_total += endpoint_count;
            if (r.failed() || d.id == 0 || endpoints_total > kMaxDiscoveryItems || endpoint_count * kMinEndpoint > r.remaining()) { r.fail(); break; }
            if (!device_ids.insert(d.id).second || !device_names.insert(d.name).second) { r.fail(); break; }
            std::set<std::uint64_t> endpoint_ids;
            std::set<std::string> endpoint_names;
            for (std::size_t k = 0; k < endpoint_count && !r.failed(); ++k) {
                DiscoveredEndpoint e;
                e.id = r.u64();
                e.name = r.name();
                const std::uint8_t direction = r.u8();
                const std::size_t capability_count = r.u16();
                if (r.failed() || e.id == 0 || direction > 1 || capability_count < 1 || capability_count > kMaxCapabilitiesPerEndpoint ||
                    capability_count * kMinCapability > r.remaining()) { r.fail(); break; }
                if (!endpoint_ids.insert(e.id).second || !endpoint_names.insert(e.name).second) { r.fail(); break; }
                e.direction = static_cast<WireDirection>(direction);
                for (std::size_t c = 0; c < capability_count && !r.failed(); ++c) {
                    DiscoveredCapability cap;
                    cap.id = r.u64();
                    cap.name = r.name();
                    if (cap.id == 0) r.fail();
                    e.capabilities.push_back(std::move(cap));
                }
                d.endpoints.push_back(std::move(e));
            }
            p.devices.push_back(std::move(d));
        }
    }
    return finish(r, std::move(p));
}

template <> Decoded<ConfigureRequestPayload> decode<ConfigureRequestPayload>(ByteSpan payload) {
    constexpr std::size_t kMinSetting = 2 + 1 + 1 + 1;     // key length + 1 key byte + type + smallest value
    ByteReader r(payload);
    ConfigureRequestPayload p;
    p.address = read_address(r);
    const std::size_t count = r.u16();
    if (r.failed() || count > kMaxConfigureSettings || count * kMinSetting > r.remaining()) r.fail();
    for (std::size_t i = 0; i < count && !r.failed(); ++i) {
        ConfigureSetting s;
        s.key = r.name();
        const std::uint8_t type = r.u8();
        if (type == 0) { const std::uint8_t b = r.u8(); if (b > 1) r.fail(); s.value = (b == 1); }
        else if (type == 1) s.value = r.i64();
        else if (type == 2) s.value = r.text();
        else r.fail();
        p.settings.push_back(std::move(s));
    }
    return finish(r, std::move(p));
}

template <> Decoded<LifecycleResponsePayload> decode<LifecycleResponsePayload>(ByteSpan payload) {
    ByteReader r(payload);
    LifecycleResponsePayload p;
    read_status(r, p.status);
    if (p.status.ok()) {
        const std::uint8_t state = r.u8();
        if (state > kMaxLifecycle) r.fail();
        p.state = static_cast<core::LifecycleState>(state);
    }
    return finish(r, std::move(p));
}

Decoded<ReadResponsePayload> decode_read_response(ByteSpan payload, ReadKind kind) {
    ByteReader r(payload);
    ReadResponsePayload p;
    p.kind = kind;
    read_status(r, p.status);
    if (p.status.ok()) {
        if (kind == ReadKind::VEC3) { p.x = r.f64(); p.y = r.f64(); p.z = r.f64(); } else { p.value = r.f64(); }
        p.sample_sequence = r.u64();
        p.timestamp_ns = r.i64();
    }
    return finish(r, std::move(p));
}

template <> Decoded<WriteRequestPayload> decode<WriteRequestPayload>(ByteSpan payload) {
    ByteReader r(payload);
    WriteRequestPayload p;
    p.address = read_address(r);
    p.velocity_rad_s = r.f64();
    return finish(r, std::move(p));
}

template <> Decoded<StatusPayload> decode<StatusPayload>(ByteSpan payload) {
    ByteReader r(payload);
    StatusPayload p;
    read_status(r, p.status);
    return finish(r, std::move(p));
}

Decoded<StatusPayload> decode_protocol_error(ByteSpan payload) {
    auto d = decode<StatusPayload>(payload);
    if (d.ok() && d.value.status.ok()) return bad_payload<StatusPayload>();      // a PROTOCOL_ERROR status is never OK
    return d;
}

template <> Decoded<ObserveResponsePayload> decode<ObserveResponsePayload>(ByteSpan payload) {
    ByteReader r(payload);
    ObserveResponsePayload p;
    read_status(r, p.status);
    if (p.status.ok()) {
        const std::uint8_t state = r.u8(), status_code = r.u8(), health = r.u8();
        if (state > kMaxLifecycle || status_code > kMaxStatusCode || health > kMaxHealth) r.fail();
        p.state = static_cast<core::LifecycleState>(state);
        p.status_code = static_cast<core::StatusCode>(status_code);
        p.health = static_cast<core::HealthState>(health);
        p.health_detail = r.text();
        p.operations_ok = r.u64();
        p.operations_failed = r.u64();
        const std::uint8_t has = r.u8();
        if (has > 1) r.fail();
        p.has_last_error = has == 1;
        if (p.has_last_error) {
            const auto code = from_wire_status(r.u16());
            if (!code.has_value()) r.fail(); else p.last_error_code = *code;
            p.last_error_message = r.text();
        }
    }
    return finish(r, std::move(p));
}

template <> Decoded<FaultEventPayload> decode<FaultEventPayload>(ByteSpan payload) {
    ByteReader r(payload);
    FaultEventPayload p;
    p.address = read_address(r);
    const std::uint8_t state = r.u8();
    if (state != static_cast<std::uint8_t>(core::LifecycleState::FAULT)) r.fail();   // a FAULT_EVENT reports FAULT
    p.reason = r.text();
    return finish(r, std::move(p));
}

template <> Decoded<HeartbeatPayload> decode<HeartbeatPayload>(ByteSpan payload) {
    ByteReader r(payload);
    HeartbeatPayload p;
    p.sender_time_ns = r.u64();
    return finish(r, std::move(p));
}

} // namespace kritva::hardware::transport
