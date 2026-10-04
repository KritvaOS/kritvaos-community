# KOS-I4 Nexus–Edge Protocol Specification

**Status:** I4-001 contract, version 1.0. It is the entry gate for I4-002 (codec): the codec is mechanical from this document, and the contract headers and tests in `hardware/transport/` and `hardware/remote/` are checked against it (`kritva_protocol_spec_consistency`).
**Dependency:** `kritva-core` R1.0 (unchanged). **Decisions:** `planning/DECISIONS.md` (KOS-I4 sections).

## 1. Scope

A hand-written, versioned, little-endian binary protocol between a Nexus and an Edge, carried by an abstract `Transport`. It is software-only and transport-independent: nothing here depends on delivery being simulated. It carries the I3 Device/Endpoint operations; it does not change them. Out of scope: EtherCAT, CAN, sockets, serialization libraries, authentication/encryption (a later milestone must add them before any real deployment).

## 2. Conventions

- All multi-byte fields are **little-endian**. `u8/u16/u32/u64` are unsigned; `i64` is two's complement; `f64` is IEEE-754 binary64.
- **No raw struct serialization**: every field is written and read individually at the offsets below.
- **`f64` values on the wire must be finite.** A NaN or infinity anywhere in a payload makes the payload malformed.
- **Strings** are a `u16` byte length followed by that many bytes, no terminator. *Name strings* (device, endpoint, setting, capability names) are 1..64 bytes of `[a-z0-9_]`. *Text strings* (messages, health details) are 0..256 bytes of safe printable UTF-8: valid UTF-8 (no overlong forms, surrogates or values above U+10FFFF) with no control characters (below 0x20 or 0x7F).
- A length or count larger than its limit (section 3) makes the frame malformed. Trailing bytes after a payload's last field make it malformed.

## 3. Constants

| Name | Value |
|---|---|
| `MAGIC` | `0x4B344F53` (the characters `K4OS`); on the wire, bytes `53 4F 34 4B` |
| `PROTOCOL_MAJOR` / `PROTOCOL_MINOR` | 1 / 0 |
| `HEADER_SIZE` | 44 bytes |
| `MAX_FRAME_SIZE` | 65536 bytes |
| `MAX_PAYLOAD_SIZE` | 65492 bytes (`MAX_FRAME_SIZE - HEADER_SIZE`) |
| `MAX_NAME_LENGTH` | 64 bytes |
| `MAX_TEXT_LENGTH` | 256 bytes |
| `MAX_DEVICES` | 64 per node |
| `MAX_DISCOVERY_ITEMS` | 256 endpoints per node (all devices together) |
| `MAX_CAPABILITIES_PER_ENDPOINT` | 16 |
| `MAX_CONFIGURE_SETTINGS` | 16 per request |

Note: the planning baseline gave `MAX_PAYLOAD_SIZE` as 65520, which assumed a 16-byte header; with the 44-byte header required by the agreed field set the payload limit is `MAX_FRAME_SIZE - HEADER_SIZE`.

## 4. Frame

A frame is a 44-byte header followed by `payload_length` bytes. The total length is exactly `HEADER_SIZE + payload_length` and at most `MAX_FRAME_SIZE`.

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | `magic` | must equal `MAGIC` |
| 4 | 2 | `protocol_major` | must equal 1 |
| 6 | 2 | `protocol_minor` | must be <= 0 in v1.0 (peer minor <= supported minor) |
| 8 | 2 | `message_type` | one of section 6 |
| 10 | 2 | `flags` | must be 0 (no flags defined in 1.0) |
| 12 | 2 | `header_length` | must equal 44 in 1.x |
| 14 | 2 | `reserved` | must be 0 |
| 16 | 4 | `payload_length` | 0..`MAX_PAYLOAD_SIZE`; must match the bytes that follow |
| 20 | 8 | `sequence` | sender's frame number, section 10 |
| 28 | 8 | `correlation_id` | in a response, the `sequence` of the request it answers; 0 otherwise |
| 36 | 8 | `session_id` | 0 only in HELLO and HELLO_ACK-error; otherwise the session's non-zero id |
| 44 | n | `payload` | layout by `message_type` |

### Validation order (an incoming frame)

1. Fewer than 44 bytes, or more than `MAX_FRAME_SIZE`: `TRUNCATED` / `OVERSIZE`.
2. `magic`: `BAD_MAGIC`.
3. `protocol_major != 1` or `protocol_minor > supported`: `UNSUPPORTED_VERSION`.
4. `header_length != 44`, `flags != 0`, `reserved != 0`: `BAD_HEADER`.
5. `payload_length > MAX_PAYLOAD_SIZE`: `PAYLOAD_TOO_LARGE`; total length not equal to `44 + payload_length`: `LENGTH_MISMATCH`.
6. `message_type` unknown: `UNKNOWN_TYPE`.
7. Only then is the payload parsed (section 7); a violation is `BAD_PAYLOAD`.

No field is used before every earlier check has passed.

## 5. Types of frames

`message_type` is a `u16`. A request's response is the request type plus 1. `HEARTBEAT` and `FAULT_EVENT` have no response.

## 6. Message types

| Type | Name | Direction | Response |
|---|---|---|---|
| 0x0001 | HELLO | Nexus to Edge | HELLO_ACK |
| 0x0002 | HELLO_ACK | Edge to Nexus | |
| 0x0003 | DISCOVERY_REQUEST | Nexus to Edge | DISCOVERY_RESPONSE |
| 0x0004 | DISCOVERY_RESPONSE | Edge to Nexus | |
| 0x0005 | CONFIGURE_REQUEST | Nexus to Edge | CONFIGURE_RESPONSE |
| 0x0006 | CONFIGURE_RESPONSE | Edge to Nexus | |
| 0x0007 | INITIALIZE_REQUEST | Nexus to Edge | INITIALIZE_RESPONSE |
| 0x0008 | INITIALIZE_RESPONSE | Edge to Nexus | |
| 0x0009 | START_REQUEST | Nexus to Edge | START_RESPONSE |
| 0x000A | START_RESPONSE | Edge to Nexus | |
| 0x000B | STOP_REQUEST | Nexus to Edge | STOP_RESPONSE |
| 0x000C | STOP_RESPONSE | Edge to Nexus | |
| 0x000D | SHUTDOWN_REQUEST | Nexus to Edge | SHUTDOWN_RESPONSE |
| 0x000E | SHUTDOWN_RESPONSE | Edge to Nexus | |
| 0x000F | READ_REQUEST | Nexus to Edge | READ_RESPONSE |
| 0x0010 | READ_RESPONSE | Edge to Nexus | |
| 0x0011 | WRITE_REQUEST | Nexus to Edge | WRITE_RESPONSE |
| 0x0012 | WRITE_RESPONSE | Edge to Nexus | |
| 0x0013 | OBSERVE_REQUEST | Nexus to Edge | OBSERVE_RESPONSE |
| 0x0014 | OBSERVE_RESPONSE | Edge to Nexus | |
| 0x0015 | FAULT_EVENT | Edge to Nexus | none |
| 0x0016 | HEARTBEAT | both | none |
| 0x0017 | PROTOCOL_ERROR | both | none |

Direction is enforced: a request type received from the wrong side is `UNKNOWN_TYPE` for that receiver.

## 7. Payloads

**Address** (`ADDR`, 24 bytes): `u64 node_id` (non-zero), `u64 device_id` (non-zero), `u64 endpoint_id` (non-zero). `node_id` must equal the Edge's own node id (a wrong node is `INVALID_ARGUMENT`).

**Status** (`STATUS`): `u16 status` (section 8); if non-zero, followed by a text string `message`. An OK response carries no message.

| Message | Payload |
|---|---|
| HELLO | `u64 node_id` (the Nexus), `u16 major`, `u16 minor`, `u32 heartbeat_period_ms`, `u32 heartbeat_timeout_ms` |
| HELLO_ACK | `STATUS`; if OK: `u64 node_id` (the Edge), `u16 major`, `u16 minor` (negotiated), `u64 session_id`, `u32 heartbeat_period_ms`, `u32 heartbeat_timeout_ms` (as accepted) |
| DISCOVERY_REQUEST | empty |
| DISCOVERY_RESPONSE | `STATUS`; if OK: `u16 device_count` (<= 64); per device: `u64 device_id`, name `device_name`, `u16 endpoint_count`; per endpoint: `u64 endpoint_id`, name `endpoint_name`, `u8 direction` (0 sensor, 1 actuator), `u16 capability_count` (1..16); per capability: `u64 capability_id`, name `capability_name`. Total endpoints <= 256. |
| CONFIGURE_REQUEST | `ADDR`, `u16 setting_count` (<= 16); per setting: name `key`, `u8 type` (0 bool, 1 int64, 2 text), value (`u8` 0/1 / `i64` / text string) |
| CONFIGURE_RESPONSE, INITIALIZE_RESPONSE, START_RESPONSE, STOP_RESPONSE, SHUTDOWN_RESPONSE | `STATUS`; if OK: `u8 lifecycle_state` (Core `LifecycleState` value after the operation) |
| INITIALIZE_REQUEST, START_REQUEST, STOP_REQUEST, SHUTDOWN_REQUEST | `ADDR` |
| READ_REQUEST | `ADDR` |
| READ_RESPONSE | `STATUS`; if OK, by the endpoint's capability: acceleration, angular velocity: `f64 x, f64 y, f64 z, u64 sample_sequence, i64 timestamp_ns`; position: `f64 value, u64 sample_sequence, i64 timestamp_ns` |
| WRITE_REQUEST | `ADDR`, `f64 velocity_rad_s` (motor command capability only) |
| WRITE_RESPONSE | `STATUS` |
| OBSERVE_REQUEST | `ADDR` |
| OBSERVE_RESPONSE | `STATUS`; if OK: `u8 lifecycle_state`, `u8 status_code` (Core `StatusCode`), `u8 health_state` (Core `HealthState`), text `health_detail`, `u64 operations_ok`, `u64 operations_failed`, `u8 has_last_error` (0/1); if 1: `u16 last_error_code` (section 8), text `last_error_message` |
| FAULT_EVENT | `ADDR`, `u8 lifecycle_state` (FAULT), text `reason` |
| HEARTBEAT | `u64 sender_time_ns` (the sender's virtual monotonic time; informational) |
| PROTOCOL_ERROR | `STATUS` (always non-zero) |

The `timestamp_ns` field is MONOTONIC-domain virtual time as in I3. Enumerations (`direction`, `type`, Core enum values) outside their defined range make the payload malformed.

## 8. Status and error mapping

The wire `status` is the numeric value of the released Core `ErrorCode`; 0 is OK.

| Wire | Core `ErrorCode` | Typical protocol use |
|---|---|---|
| 0 | `NONE` | success |
| 1 | `UNKNOWN` | unclassified |
| 2 | `INVALID_ARGUMENT` | malformed payload, wrong node, unknown device or endpoint, unknown or invalid setting, rejected command |
| 3 | `INVALID_STATE` | operation invalid for the state, stale or unknown session |
| 4 | `NOT_INITIALIZED` | |
| 5 | `NOT_READY` | endpoint not running |
| 6 | `ALREADY_RUNNING` | |
| 7 | `TIMEOUT` | local only: a request's deadline passed (never sent as a response) |
| 8 | `RESOURCE_UNAVAILABLE` | endpoint faulted |
| 9 | `CONFIGURATION_ERROR` | settings rejected |
| 10 | `UNSUPPORTED` | unknown message or capability, unsupported version |
| 11 | `INTERNAL_ERROR` | the Edge endpoint reported a hardware failure |

A status value above 11 makes the payload malformed. The Edge returns the I3 endpoint's own `ErrorCode` and message unchanged.

## 9. Sessions

- A **HELLO** (`session_id` 0) proposes a version and the heartbeat timing. The Edge accepts if `major == 1` and the peer's `minor <= its own minor`; the negotiated minor is the peer's. It replies HELLO_ACK with a new non-zero `session_id` (the Edge allocates 1, 2, 3 ... per EdgeHost lifetime), the Edge node id and the accepted timing. Timing outside the ranges in section 14 is `INVALID_ARGUMENT`; an unsupported version is `UNSUPPORTED` (HELLO_ACK with `session_id` 0).
- Every later frame in both directions carries that `session_id`; a frame with any other session id is stale (section 10).
- **A HELLO always starts a new session and invalidates the previous one.** The Edge then stops every actuator endpoint of the old session through the existing `Endpoint::stop()`, resets all sequence ledgers, and seals the served Devices (idempotent).
- Session states (a Nexus-side link state; **not** Core `LifecycleState`):

| From | To | Cause |
|---|---|---|
| DISCONNECTED | CONNECTING | `connect()`: transport up, HELLO sent |
| CONNECTING | NEGOTIATING | valid HELLO_ACK received |
| CONNECTING | DISCONNECTED | rejected HELLO, version mismatch, request timeout, transport disconnect |
| NEGOTIATING | CONNECTED | valid DISCOVERY_RESPONSE received |
| NEGOTIATING | DISCONNECTED | discovery error or timeout, transport disconnect |
| CONNECTED | DEGRADED | no valid peer frame for 2 heartbeat periods |
| DEGRADED | CONNECTED | a valid peer frame arrives before the timeout |
| CONNECTED | DISCONNECTED | heartbeat timeout, transport disconnect, explicit close |
| DEGRADED | DISCONNECTED | heartbeat timeout, transport disconnect, explicit close |

Every other transition is invalid and changes nothing. DISCONNECTED is also the initial state. The only way out of DISCONNECTED is a new `connect()` (no automatic reconnect).

## 10. Sequence, correlation, duplicates and stale frames

- Each side numbers its frames in a session `1, 2, 3 ...`: strictly increasing, gaps allowed, counted per sender; the first frame of a session is 1. A HELLO uses sequence 1 of its own counter.
- A **response** carries `correlation_id` = the request's `sequence`. A request has `correlation_id` 0.
- The receiver keeps the highest accepted `sequence` per peer. A frame with `sequence <=` that value, or a wrong `session_id`, is **stale or duplicate**: it is dropped and counted, never processed. The single exception is a duplicate WRITE_REQUEST (section 12).
- A response is accepted only if it matches an outstanding request by correlation id, expected response type, session and address; any other response (unknown correlation, wrong type, wrong address, already answered, after the deadline) is dropped and counted.
- Each peer counts, per category: malformed, stale, duplicate, unknown correlation, late.

## 11. Deadlines and heartbeat

- Every request has a deadline = send time + `link.request_timeout_ms` on the virtual clock. A response whose delivery time is after the deadline is late (dropped and counted); the request completes with `TIMEOUT`. The Nexus never retransmits automatically: a write that times out has an **unknown outcome** and must be treated as such by the application.
- Both sides send a HEARTBEAT every `link.heartbeat_period_ms` once the session exists. Any valid in-session frame counts as proof of liveness. No valid frame from the peer for `link.heartbeat_timeout_ms` ends the session: the Nexus link goes DISCONNECTED and every remote endpoint becomes FAULT (reason `link lost`, one ERROR per endpoint, section 12 of the architecture); the Edge invalidates the session, stops **all** actuator endpoints of that session through `Endpoint::stop()`, and does not itself fault any endpoint.
- Time is the transport's virtual monotonic nanosecond clock, advanced only by `step(dt)`/`advance(dt)`. A synchronous call pumps in `link.pump_quantum_ms` steps until the response or its deadline.

## 12. Edge-authoritative writes

For each session and actuator endpoint the Edge keeps `last_applied_sequence` and the cached last WRITE_RESPONSE. On WRITE_REQUEST (after frame validation and in this order):

1. session must be the current one, else stale (dropped);
2. the address must be valid, the endpoint an actuator, and the capability must be the motor command, else `INVALID_ARGUMENT`;
3. `sequence == last_applied_sequence` and an identical payload: **resend the cached response**, do not apply;
4. `sequence <= last_applied_sequence` otherwise: stale, dropped;
5. the endpoint must be RUNNING (`NOT_READY`; faulted: `RESOURCE_UNAVAILABLE`);
6. the command is validated by the I3 endpoint (finite, within its limits) **before** it is applied; a rejected command changes nothing;
7. apply once, record `last_applied_sequence = sequence`, cache the response, respond.

The Edge never trusts the Nexus for limits or state. A new session starts a new ledger, so a retransmission from an old session can never be applied.

## 13. Discovery, typed mapping and topology equivalence

- Discovery is a complete snapshot, sent in answer to DISCOVERY_REQUEST (the Nexus requests it after HELLO and again on each initialize). The Edge topology is sealed from the first HELLO; there is no change notification.
- Duplicate device ids or names, or duplicate endpoint ids or names inside one device, make a discovery response malformed. Device ids may collide across different nodes; the Nexus keys everything by `(node_id, device_id, endpoint_id)`.
- The capability id selects the typed proxy: `0x1001` acceleration, `0x1002` angular velocity, `0x1003` position, `0x2001` motor command (the I3 capability ids). A sensor endpoint must advertise exactly one of the first three and an actuator exactly the last; any other id or combination is `UNSUPPORTED` and discovery fails.
- **Topology equivalence** (registered snapshot versus a fresh discovery): the sets are equal iff they contain the same `(node_id, device_id, device_name, endpoint_id, endpoint_name, direction, capability ids)` tuples. Order is not part of identity. Any difference fails deterministically.

## 14. Link timing (configuration keys, defaults, ranges)

| Key | Default | Range |
|---|---|---|
| `link.heartbeat_period_ms` | 100 | 10..60000 |
| `link.heartbeat_timeout_ms` | 300 | 20..600000, and >= 2 x period |
| `link.request_timeout_ms` | 100 | 1..60000 |
| `link.pump_quantum_ms` | 1 | 1..1000, and <= request timeout |

The pump quantum is a simulation/control-plane parameter, not a protocol timing guarantee.

## 15. Malformed, unknown and over-size input

- Header violations (section 4, steps 1 to 6) drop the frame silently; it is counted. Nothing is sent back, so a bad sender cannot cause amplification.
- A frame with a valid header and a known **request** type but a malformed payload is answered with PROTOCOL_ERROR (`INVALID_ARGUMENT`, correlation id = its sequence), at most once per frame.
- A malformed **response** that matches an outstanding request completes that request with `INVALID_ARGUMENT` (not a timeout); otherwise it is dropped.
- A request from the wrong side, and any unknown type, is dropped (`UNKNOWN_TYPE`).
- The decoder never allocates from a length field before the length has been checked against the limits.

## 16. Versioning

Within major 1, a peer with a smaller or equal minor is compatible. A minor increment may only add message types, flags and trailing optional behaviour that older peers can ignore by the rules above; it never changes a layout in this document. A different major is incompatible.

## 17. I4-001 traceability

| Requirement | Section |
|---|---|
| NDR-001..003 | 7 (ADDR), 9, 13 |
| PR-001..003 | 6, 7, 9, 10 |
| FR-001..003 | 2, 3, 4 |
| TR-003 | 14 |
| ER-002, ER-003 | 12, 13 |
| FRL-002 | 10, 11, 15 |
| SR-001, SR-002, SR-003 | 11, 12 |
