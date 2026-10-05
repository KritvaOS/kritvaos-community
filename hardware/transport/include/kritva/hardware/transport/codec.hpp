//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : codec.hpp
// Description : Bounds-checked little-endian byte codec and the typed payload encoders and decoders of the Nexus-Edge protocol.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Transport Codec
// Layer       : Hardware Abstraction
//
// Requirements: FR-002; FR-003; PR-002
// API         : kritva::hardware::transport::ByteWriter / ByteReader / encode / decode
//
// Author      : KritvaOS
// Created     : 05-10-2026
//==============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <kritva/hardware/transport/frame.hpp>

// The codec implements docs/architecture/KOS-I4_PROTOCOL.md sections 2 and 7 and nothing else: it makes no
// session, sequence, correlation, address-ownership or capability-mapping decision (those belong to the
// session layer, I4-003 onward). It depends on Core and on the I4-001 contract only: not on a transport, a
// remote proxy, the hardware abstraction or an Edge service.
namespace kritva::hardware::transport {

// ---- byte writer and reader -----------------------------------------------------------------------

/// Appends little-endian fields to a buffer that can never exceed `limit` bytes. After the first failure
/// (a limit overrun or a non-finite f64) every further write is ignored and ok() is false.
class ByteWriter {
public:
    explicit ByteWriter(std::size_t limit = kMaxPayloadSize) noexcept : limit_(limit) {}

    void u8(std::uint8_t v) { put(v, 1); }
    void u16(std::uint16_t v) { put(v, 2); }
    void u32(std::uint32_t v) { put(v, 4); }
    void u64(std::uint64_t v) { put(v, 8); }
    void i64(std::int64_t v) { put(static_cast<std::uint64_t>(v), 8); }
    /// IEEE-754 binary64, little-endian. A NaN or infinity fails the writer (nothing non-finite reaches the wire).
    void f64(double v);
    /// u16 byte length followed by the bytes. The caller validates the content; a length above 65535 fails.
    void string(std::string_view s);

    [[nodiscard]] bool ok() const noexcept { return ok_; }
    [[nodiscard]] std::size_t size() const noexcept { return buffer_.size(); }
    [[nodiscard]] std::vector<std::uint8_t> take() { return std::move(buffer_); }

private:
    void put(std::uint64_t value, std::size_t size);

    std::vector<std::uint8_t> buffer_;
    std::size_t limit_;
    bool ok_{true};
};

/// Reads little-endian fields from an explicitly bounded span; it can never read outside it, whatever the
/// input claims. After the first failure (a read past the end, a non-finite f64, an invalid string) every
/// further read returns zero/empty and failed() is true. remaining() is exact, so a decoder can require exact
/// consumption.
class ByteReader {
public:
    explicit ByteReader(ByteSpan bytes) noexcept : bytes_(bytes) {}

    std::uint8_t u8() { return static_cast<std::uint8_t>(take(1)); }
    std::uint16_t u16() { return static_cast<std::uint16_t>(take(2)); }
    std::uint32_t u32() { return static_cast<std::uint32_t>(take(4)); }
    std::uint64_t u64() { return take(8); }
    std::int64_t i64() { return static_cast<std::int64_t>(take(8)); }
    /// A finite binary64; a NaN or infinity fails the reader.
    double f64();
    /// A name string: 1..64 bytes of [a-z0-9_].
    std::string name();
    /// A text string: 0..256 bytes of safe printable UTF-8 (valid UTF-8, no control characters).
    std::string text();

    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - position_; }
    [[nodiscard]] bool failed() const noexcept { return failed_; }
    /// Fails the reader (used by decoders for a semantic-syntax violation such as an out-of-range enum).
    void fail() noexcept { failed_ = true; }

private:
    std::uint64_t take(std::size_t size);
    std::string string(std::size_t min, std::size_t max, bool is_name);

    ByteSpan bytes_;
    std::size_t position_{0};
    bool failed_{false};
};

/// Validators shared by the writer side and the reader side (spec section 2).
[[nodiscard]] bool is_valid_wire_name(std::string_view s) noexcept;   // 1..64 bytes of [a-z0-9_]
[[nodiscard]] bool is_valid_wire_text(std::string_view s) noexcept;   // 0..256 bytes, strict UTF-8, no control characters

// ---- payloads (spec section 7) ----------------------------------------------------------------------

/// STATUS: a wire status (the Core ErrorCode value) and, when it is not OK, a text message.
struct StatusField {
    core::ErrorCode code{core::ErrorCode::NONE};
    std::string message;                              // empty and not encoded when code == NONE
    [[nodiscard]] bool ok() const noexcept { return code == core::ErrorCode::NONE; }
};

/// ADDR: node, device and endpoint ids, each non-zero.
struct AddressPayload {
    std::uint64_t node_id{0};
    std::uint64_t device_id{0};
    std::uint64_t endpoint_id{0};
};

struct HelloPayload {
    std::uint64_t node_id{0};                          // the Nexus, non-zero
    std::uint16_t major{kProtocolMajor};
    std::uint16_t minor{kProtocolMinor};
    std::uint32_t heartbeat_period_ms{0};
    std::uint32_t heartbeat_timeout_ms{0};
};

struct HelloAckPayload {
    StatusField status;
    std::uint64_t node_id{0};                          // the Edge, non-zero (only when status is OK)
    std::uint16_t major{kProtocolMajor};
    std::uint16_t minor{kProtocolMinor};
    std::uint64_t session_id{0};                       // non-zero (only when status is OK)
    std::uint32_t heartbeat_period_ms{0};
    std::uint32_t heartbeat_timeout_ms{0};
};

enum class WireDirection : std::uint8_t { SENSOR = 0, ACTUATOR = 1 };

struct DiscoveredCapability {
    std::uint64_t id{0};
    std::string name;
};

struct DiscoveredEndpoint {
    std::uint64_t id{0};
    std::string name;
    WireDirection direction{WireDirection::SENSOR};
    std::vector<DiscoveredCapability> capabilities;    // 1..16 on the wire
};

struct DiscoveredDevice {
    std::uint64_t id{0};
    std::string name;
    std::vector<DiscoveredEndpoint> endpoints;
};

/// Duplicate device ids or names, and duplicate endpoint ids or names within one device, make it malformed.
struct DiscoveryResponsePayload {
    StatusField status;
    std::vector<DiscoveredDevice> devices;             // <= 64 devices, <= 256 endpoints in total (only when status is OK)
};

using SettingValue = std::variant<bool, std::int64_t, std::string>;

struct ConfigureSetting {
    std::string key;                                   // a name
    SettingValue value;                                // bool (type 0), int64 (type 1), text (type 2)
};

struct ConfigureRequestPayload {
    AddressPayload address;
    std::vector<ConfigureSetting> settings;            // <= 16
};

/// CONFIGURE/INITIALIZE/START/STOP/SHUTDOWN responses: STATUS and, when OK, the lifecycle state after the operation.
struct LifecycleResponsePayload {
    StatusField status;
    core::LifecycleState state{core::LifecycleState::UNKNOWN};
};

enum class ReadKind : std::uint8_t { VEC3, SCALAR };   // acceleration and angular velocity, or position

/// READ_RESPONSE. The layout depends on the endpoint's capability, which only the requester knows: it passes the kind to decode.
struct ReadResponsePayload {
    StatusField status;
    ReadKind kind{ReadKind::VEC3};
    double x{0}, y{0}, z{0};                           // VEC3
    double value{0};                                   // SCALAR
    std::uint64_t sample_sequence{0};
    std::int64_t timestamp_ns{0};
};

struct WriteRequestPayload {
    AddressPayload address;
    double velocity_rad_s{0};                          // finite
};

/// WRITE_RESPONSE and PROTOCOL_ERROR carry only a STATUS (a PROTOCOL_ERROR status is never OK).
struct StatusPayload {
    StatusField status;
};

struct ObserveResponsePayload {
    StatusField status;
    core::LifecycleState state{core::LifecycleState::UNKNOWN};
    core::StatusCode status_code{core::StatusCode::UNKNOWN};
    core::HealthState health{core::HealthState::UNKNOWN};
    std::string health_detail;                         // text
    std::uint64_t operations_ok{0};
    std::uint64_t operations_failed{0};
    bool has_last_error{false};
    core::ErrorCode last_error_code{core::ErrorCode::NONE};
    std::string last_error_message;                    // text
};

struct FaultEventPayload {
    AddressPayload address;
    core::LifecycleState state{core::LifecycleState::FAULT};   // must be FAULT
    std::string reason;                                // text
};

struct HeartbeatPayload {
    std::uint64_t sender_time_ns{0};
};

// ---- encoders and decoders ---------------------------------------------------------------------------

/// The outcome of decoding one payload: NONE with a value, or BAD_PAYLOAD. A decoder validates every field
/// (finite numbers, strict strings, enum ranges, counts against their limits and against the bytes that
/// remain BEFORE iterating or allocating, no duplicates where the spec forbids them) and requires that the
/// payload is consumed exactly: trailing bytes and truncation are both BAD_PAYLOAD.
template <class T>
struct Decoded {
    FrameError error{FrameError::NONE};
    T value{};
    [[nodiscard]] bool ok() const noexcept { return error == FrameError::NONE; }
};

/// Encoders return the payload bytes, or INVALID_ARGUMENT if the value breaks any rule a decoder would enforce
/// (so everything that can be encoded decodes back to an equal value) or would not fit MAX_PAYLOAD_SIZE; the
/// required size is computed before any encoding where it can exceed the frame (discovery).
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode(const AddressPayload&);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode(const HelloPayload&);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode(const HelloAckPayload&);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode(const DiscoveryResponsePayload&);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode(const ConfigureRequestPayload&);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode(const LifecycleResponsePayload&);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode(const ReadResponsePayload&);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode(const WriteRequestPayload&);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode(const StatusPayload&);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode(const ObserveResponsePayload&);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode(const FaultEventPayload&);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode(const HeartbeatPayload&);

/// The exact size of the encoded DISCOVERY_RESPONSE payload, computed without encoding (INVALID_ARGUMENT for an invalid value).
[[nodiscard]] core::Result<std::size_t> encoded_size(const DiscoveryResponsePayload&);

/// Decodes the payload of the message type that carries T. A payload of a message with no fields (DISCOVERY_REQUEST) is
/// checked with decode_empty().
template <class T> [[nodiscard]] Decoded<T> decode(ByteSpan payload);
[[nodiscard]] Decoded<ReadResponsePayload> decode_read_response(ByteSpan payload, ReadKind kind);
[[nodiscard]] FrameError decode_empty(ByteSpan payload) noexcept;

/// PROTOCOL_ERROR: a STATUS payload whose status is never OK (the shared StatusPayload also serves WRITE_RESPONSE, where OK is legal).
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encode_protocol_error(const StatusPayload&);
[[nodiscard]] Decoded<StatusPayload> decode_protocol_error(ByteSpan payload);

#define KRITVA_DECLARE_DECODE(T) template <> [[nodiscard]] Decoded<T> decode<T>(ByteSpan);
KRITVA_DECLARE_DECODE(AddressPayload)
KRITVA_DECLARE_DECODE(HelloPayload)
KRITVA_DECLARE_DECODE(HelloAckPayload)
KRITVA_DECLARE_DECODE(DiscoveryResponsePayload)
KRITVA_DECLARE_DECODE(ConfigureRequestPayload)
KRITVA_DECLARE_DECODE(LifecycleResponsePayload)
KRITVA_DECLARE_DECODE(WriteRequestPayload)
KRITVA_DECLARE_DECODE(StatusPayload)
KRITVA_DECLARE_DECODE(ObserveResponsePayload)
KRITVA_DECLARE_DECODE(FaultEventPayload)
KRITVA_DECLARE_DECODE(HeartbeatPayload)
#undef KRITVA_DECLARE_DECODE

} // namespace kritva::hardware::transport
