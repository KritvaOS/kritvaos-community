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
| `MAX_DISCOVERY_PAYLOAD` | 43524 bytes: the largest legal DISCOVERY_RESPONSE payload (derived in section 13) |

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
| DISCOVERY_RESPONSE | `STATUS`; if OK: `u16 device_count` (<= 64); per device: `u64 device_id`, name `device_name`, `u16 endpoint_count`; per endpoint: `u64 endpoint_id`, name `endpoint_name`, `u8 direction` (0 sensor, 1 actuator), `u16 capability_count` (1..16 on the wire; protocol 1.0 requires exactly 1, section 13); per capability: `u64 capability_id`, name `capability_name`. Total endpoints <= 256. |
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
- **Only an ACCEPTED HELLO changes session state.** A rejected HELLO (a header with `session_id` other than 0 or `sequence` other than 1, a malformed payload, an unsupported version, timing out of range) is answered once, unsessioned (`session_id` 0, `correlation_id` = the HELLO's sequence), and has **no observable effect** on the current session: not its identity, the actuator state, the sequence tracking, the write ledgers, the topology or any lifecycle state. Otherwise a malformed or incompatible HELLO would be a remote actuator-stop primitive.
- **A HELLO always starts a new session and invalidates the previous one. This is an Edge-side safety action, not only an identifier replacement**, performed in this order: (1) invalidate the previous `SessionId`; (2) stop every actuator endpoint that belongs to the previous session through the existing `Endpoint::stop()`; (3) reset the per-session sequence tracking and all write ledgers; (4) establish the new `SessionId` and seal the served Devices (idempotent). The new session inherits **no** sequence or write-ledger state from the previous one. Step (2) applies when there was a previous session; it stops the actuator endpoints that are RUNNING (a READY or STOPPED endpoint is not driving anything); an endpoint that fails to stop is FAULT as the I3 contract defines. Sensor endpoints are not touched.
- **HELLO is not idempotent.** Protocol 1.0 has no HELLO replay or deduplication: every accepted HELLO establishes a fresh session. A HELLO delivered twice (for example duplicated by the link) therefore replaces the session it just created; the peer that holds the earlier session id finds all its frames stale and must fail deterministically. Peers must not assume HELLO is idempotent.
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

Every other transition is invalid and changes nothing. DISCONNECTED is also the initial state. DEGRADED means exactly: no valid peer frame for 2 heartbeat periods. A valid frame received while DEGRADED returns the link to CONNECTED. **DEGRADED never stops any actuator by itself**; the safety actions (Nexus remote endpoints to FAULT, Edge actuator stop) happen only when the heartbeat timeout is reached, which ends the session in DISCONNECTED. DISCONNECTED never returns to CONNECTING automatically: a new session needs an explicit application `connect()` (no automatic reconnect).

## 10. Sequence, correlation, duplicates and stale frames

- Each side numbers its frames in a session `1, 2, 3 ...`: strictly increasing, gaps allowed, counted per sender; the first frame of a session is 1. A HELLO uses sequence 1 of its own counter.
- A **response** carries `correlation_id` = the request's `sequence`. A request has `correlation_id` 0.
- **Two admission rules, by kind of frame.** Frame sequence numbers detect duplicate and stale **request and notice** frames; they do not impose an order between independently outstanding **responses**, because the transport may reorder frames and a response may legitimately arrive after a later heartbeat.
  - *Requests and notices* (every request type, HEARTBEAT and FAULT_EVENT, and PROTOCOL_ERROR with `correlation_id` 0): the receiver keeps the highest accepted `sequence` of these frames **per peer direction and per session** (never globally; a new session starts again from nothing). A frame with `sequence <=` that value, or a wrong `session_id`, is **stale or duplicate**: it is dropped and counted, never processed. The single exception is a duplicate WRITE_REQUEST (section 12).
  - *Responses* (every response type, HELLO_ACK, and PROTOCOL_ERROR with a non-zero `correlation_id`): a response is validated by session, `correlation_id`, expected response type, address and outstanding-request state. It is **not** rejected because its `sequence` is lower than a later frame already accepted, and it neither raises nor is compared with the request/notice watermark. Example: the Edge sends WRITE_RESPONSE (sequence 10) then HEARTBEAT (sequence 11); the transport delivers the heartbeat first; the response with sequence 10 is still admitted if request 7 is outstanding and unexpired, whereas a *request* with sequence 10 arriving after request 11 is stale.
- A response is accepted only if it matches an outstanding request by correlation id, expected response type, session and address; any other response (unknown correlation, wrong type, wrong address, already answered, after the deadline) is dropped and counted. A duplicate response is dropped because its request is no longer outstanding.
- Each peer counts, per category: malformed, stale, duplicate, unknown correlation, late.

## 11. Deadlines and heartbeat

- Every request has a deadline = send time + `link.request_timeout_ms` on the virtual clock. A response whose delivery time is after the deadline is late (dropped and counted); the request completes with `TIMEOUT`. The Nexus never retransmits automatically. The guarantee is **at-most-once application at the Edge; the outcome may be UNKNOWN to the Nexus application after a request timeout** (a timed-out write may or may not have been applied, and the application must treat it as unknown). It is not end-to-end exactly-once.
- **Heartbeat and supervision (I4-006).** Once a session exists both sides send a HEARTBEAT (a notice carrying the sender's virtual time) every `link.heartbeat_period_ms`: at most one per drive call, never a catch-up burst after a jump in time. A heartbeat only reports liveness; it never starts, restarts, recovers or clears anything. Supervision is evaluated against the virtual clock **only when it is driven** (`EdgeHost::poll()`, `RemoteNode::service()`, and every step of the synchronous request pump): there is no timer, thread, sleep or wall clock, so a gap in driving is a gap in supervision. Frames that are due now are handled before supervision decides.
  - **Liveness.** `last_valid_frame` is the virtual time of the last frame that counts. If `now - last_valid_frame >= link.heartbeat_timeout_ms` (the comparison is `>=`: exactly at the timeout, not after it) the session ends. No liveness for 2 heartbeat periods is DEGRADED: observation only, it never acts, and a valid frame returns to CONNECTED.
  - **Edge, on timeout (the final actuator-safety authority).** One transition: the session id becomes permanently invalid (it never regains validity; a new HELLO always gets a new id), the sequence watermark and every write ledger are cleared, every RUNNING actuator endpoint is stopped through the existing `Endpoint::stop()`, and the Edge then awaits a HELLO, dropping everything else. Sensor endpoints keep their I3 state and no Edge endpoint is faulted by the loss of the link. A FAULT_EVENT is not told for a fault that happens while the session is being ended or replaced (there is no session to tell). Edge supervision protects the physical actuator; Nexus supervision protects the correctness of the remote representation.
  - **Nexus, on DISCONNECTED from CONNECTED or DEGRADED** (heartbeat timeout, the transport going down, or an explicit close): every live remote endpoint (READY or RUNNING) goes FAULT through the existing `Endpoint::enter_fault` with the reason `link lost` (`session closed` for an explicit close), giving one ERROR per endpoint through the DeviceManager's existing fault-listener path; an endpoint that is UNKNOWN, STOPPED or already FAULT gets nothing; and **one** link-level diagnostic record is kept per loss. For N live endpoints: exactly N endpoint faults, N ERROR notifications and 1 record, and driving the session again after the loss adds none. A handshake that fails is not a link loss. An explicit close is a Nexus-local event: protocol 1.0 has no termination message, so the Edge learns of it only by its own timeout.
  - **Recovery** is never automatic: no reconnect, no restart, no clearing of a FAULT. The way back is shutdown, initialize, start with a fresh HELLO (and the Nexus-side cleanup of section 9 of the remote architecture); a fresh HELLO never turns a FAULT endpoint back to READY.
  - **FAULT_EVENT.** When a served Edge endpoint enters FAULT (a transition: FAULT to FAULT is not one) the Edge sends one FAULT_EVENT (address, state FAULT, the endpoint's last error as the reason), queued so that it follows the response of the request being handled, or sent at the next drive for a fault that happened outside a request; none if there is no session. The Nexus admits it as a notice (session, payload, watermark, expected node) and puts the matching live remote endpoint FAULT with the reason `remote fault: <reason>`, so that a remote fault is distinguishable from `link lost` and `session closed`; an endpoint that is not live or already FAULT gets no second event. A FAULT_EVENT is information only: losing one costs no safety (the Edge endpoint is itself FAULT and refuses), and receiving one never causes an Edge safety action.
  - **Which frames refresh liveness.** Liveness is refreshed only by a newly admitted, currently valid protocol frame, never by the replay of earlier application traffic:

| Frame disposition at the Edge | Refreshes liveness |
|---|---|
| a new admitted request | yes |
| a new admitted notice (including a PROTOCOL_ERROR with correlation 0) | yes |
| an admitted HEARTBEAT | yes |
| a valid request the endpoint then refuses (address, state or limits) | yes |
| a duplicate WRITE answered from the ledger | **no** |
| a stale request or notice | no |
| a malformed frame (header violation or a payload that fails decoding) | no |
| a frame of another session, or of no session | no |
| a frame that cannot reach an Edge (a response type, or a response-like PROTOCOL_ERROR) | no |
| a HELLO that is rejected | no |

    "Valid" means that decoding succeeded and the frame was admitted as a new in-session request or notice; a refusal after admission does not negate it. The reason for the duplicate WRITE: a duplicate is by definition a retransmission of an old frame and no evidence that the peer is alive now, and protocol 1.0 has no authentication, so a stuck retransmitter or a captured frame repeating the last WRITE must never keep an actuator running: a live Nexus keeps sending fresh frames and heartbeats. The Nexus applies the same rule to what it receives: an accepted response, an admitted notice and a matching PROTOCOL_ERROR refresh; a late, duplicate, unknown-correlation, wrong-type, wrong-session or malformed response, a stale notice and a malformed notice do not. A frame's liveness time is the time at which it is handled; a backlog is handled when a side is driven again.
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

A stale frame (section 10) is dropped before any response, so steps 2 and 5 to 7 apply to admitted frames only; the duplicate exception of step 3 needs the ledger, which is found by the address, so a frame whose address does not resolve is never a duplicate. A rejected write (steps 2, 5, 6) does not change the ledger.

The Edge never trusts the Nexus for limits or state. A new session starts a new ledger, so a retransmission from an old session can never be applied.

## 13. Discovery, typed mapping and topology equivalence

- Discovery is a complete snapshot, sent in answer to DISCOVERY_REQUEST (the Nexus requests it after HELLO and again on each initialize). The Edge topology is sealed from the first HELLO; there is no change notification.
- Duplicate device ids or names, or duplicate endpoint ids or names inside one device, make a discovery response malformed. Device ids may collide across different nodes; the Nexus keys everything by `(node_id, device_id, endpoint_id)`.
- The capability id selects the typed proxy: `0x1001` acceleration, `0x1002` angular velocity, `0x1003` position, `0x2001` motor command (the I3 capability ids). A sensor endpoint must advertise exactly one of the first three and an actuator exactly the last; any other id or combination is `UNSUPPORTED` and discovery fails.
- **Capacity.** The wire format allows up to 16 capabilities per endpoint, but protocol 1.0 requires exactly one (above), so the largest legal discovery is: `2` (status) `+ 2` (device_count) `+ 64 x (8 + 2 + 64 + 2)` (devices: id, name length, name, endpoint_count) `+ 256 x (8 + 2 + 64 + 1 + 2 + (8 + 2 + 64))` (endpoints: id, name length, name, direction, capability_count, one capability with id, name length, name) = `2 + 2 + 4864 + 38656 = 43524` bytes, which is at most `MAX_PAYLOAD_SIZE` (65492), so a complete snapshot always fits one DISCOVERY_RESPONSE. The frame bound is the hard wire limit and the item limits are semantic limits: an encoder computes the required size first and rejects before producing an oversized frame; an Edge whose topology cannot be encoded within `MAX_PAYLOAD_SIZE` (possible only if a later minor version allows more capabilities) answers `UNSUPPORTED` and serves no topology. The decoder checks the frame bound before any item limit.
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

Within major 1, a peer with a smaller or equal minor is compatible. A minor increment may only add message types, flags and trailing optional behaviour that older peers can ignore; **these relaxations apply only once a negotiated minor version actually defines them. In version 1.0 none is defined: `flags` must be 0, `reserved` must be 0, trailing bytes after a payload are malformed and an unknown message type is dropped.** A minor increment never changes a layout in this document. A different major is incompatible.

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
