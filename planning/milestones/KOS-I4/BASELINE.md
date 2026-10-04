# KOS-I4 Planning Baseline

Status: PLANNING BASELINE. Dependency: KOS-I3 software closure complete; I3 human safety review remains OPEN independently.

## Decisions A-F

A. RemoteEndpoint operations remain synchronous. Each call sends one request and internally pumps deterministic transport until the matching response or simulated deadline. No async application callbacks.

B. Edge heartbeat/session timeout stops affected actuator endpoints using the existing I3 Endpoint::stop(). No new safe_state() hook. Recovery is explicit initialize/start.

C. SimulatedTransport owns a monotonic virtual nanosecond clock; time advances only by step(dt)/advance(dt). Defaults: heartbeat period 100 ms; heartbeat timeout 300 ms (3 periods); request timeout 100 ms. All are I3-style configuration keys.

D. Wire format: magic K4OS (u32 0x4B344F53), u16 major/u16 minor, exact-major compatibility, peer minor <= supported minor, little-endian fields, UTF-8 strings with u16 byte length. Names [a-z0-9_], 1..64 bytes. Other strings max 256 bytes safe printable UTF-8. MAX_FRAME_SIZE 65536; MAX_PAYLOAD_SIZE 65520; MAX_DISCOVERY_ITEMS 256. No raw C++ struct serialization.

E. Discovery is a complete snapshot after HELLO and on explicit request. Edge topology is sealed after DeviceManager initialization. No topology-change notification. Duplicate ids/names within a Node are rejected; DeviceId collisions across Nodes are legal and registry keys include NodeId.

F. No automatic reconnect. Recovery is shutdown -> initialize (fresh HELLO/session/discovery) -> start. New session invalidates old session state. Edge actuator duplicate ledger is reset only with a new session id, preventing old-session writes from applying.

## Prior 14 decisions

1. Software-only, Linux-hosted, simulation-first.
2. Nexus and Edge may share one process, but architecture is process-independent.
3. One deterministic SimulatedTransport; no TCP/UDP in I4.
4. Explicit stepping only; no threads, sleep or wall clock.
5. Core R1.0 unchanged.
6. Reuse Core MessageHeader, Topic and IClock where useful; I4 owns framing/protocol/transport.
7. Mandatory messages: HELLO, discovery, configure, lifecycle, read, write, status/health/statistics, fault/event, heartbeat.
8. Endpoint address is (NodeId, DeviceId, EndpointId).
9. Existing DeviceManager is reused; no RemoteDeviceManager.
10. Session state is I4-only, not Core lifecycle.
11. Nexus remote fault is a derived condition; Edge need not enter Core FAULT on link loss.
12. Edge is actuator safety authority.
13. Edge maintains last_applied_sequence/bounded duplicate ledger; sequence alone is insufficient.
14. No automatic recovery; explicit fresh session only.

## Success path

Application -> RuntimeHost -> DeviceManager -> RemoteDevice -> RemoteEndpoint -> I4 Protocol -> SimulatedTransport -> I4 Protocol -> EdgeHost -> I3 Device -> I3 Endpoint -> Mock Hardware.

Must prove HELLO/version negotiation, discovery, configure, initialize/start, read, write, diagnostics, heartbeat, link loss, Edge safe stop, Nexus FAULT, stale/duplicate rejection, and explicit recovery.

## Explicit exclusions

EtherCAT, CAN, STM32/MCU firmware, PREEMPT_RT, ROS2/DDS, real sockets, real hardware, vendor networking stacks, third-party serialization, physical actuator deployment.
