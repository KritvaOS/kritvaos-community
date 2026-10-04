# KOS-I4 Test Plan

Unit: identity, frame encode/decode, endian/length/version/string checks, sequence/ledger, session state, virtual clock, transport queue/faults.

Integration: HELLO, discovery, configure, lifecycle, read/write, diagnostics, heartbeat, link loss, timeout, explicit recovery, multi-node identity.

Negative: truncation, overflow, bad magic/version/type, malformed UTF-8, duplicate discovery items, unknown correlation, wrong endpoint, stale/duplicate response, replayed old-session write.

Safety: Edge validates limits before apply; rejected/failed writes do not alter model; heartbeat loss stops/zeros motor; stopped actuator rejects writes; old-session writes never apply.

Sanitizers: ASan and UBSan for every task gate. No sanitizer warning accepted.
