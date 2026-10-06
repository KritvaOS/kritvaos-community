# SR Requirements

See `docs/requirements/KOS-I4/REQUIREMENTS.md` for the normative traceability table.

Family: SR

## Clarifications (planning review)

Edge-authoritative actuator validation (limits, state, sequence) before every apply; a session-scoped sequence ledger (`last_applied_sequence` and the cached last response per actuator endpoint); a retransmitted identical write returns the cached response and is not applied again; a lower sequence is stale and rejected; a write from an old session id is rejected; heartbeat timeout stops ALL actuator endpoints served in that session through the existing `Endpoint::stop()` (zero/fail-safe motor); no automatic recovery; a new session starts a new ledger. The human safety review is an independent OPEN gate.

## I4-004 / I4-006 split (architect ruling)

The mechanisms of SR-001 (the endpoint validates limits and state before every apply) and SR-002 (session-scoped last-applied sequence and cached response, stale and reordered writes dropped, an accepted HELLO resetting the ledgers and stopping the previous session's running actuators) are implemented in I4-004 because the Edge write path cannot exist safely without them. I4-006 owns SR-003 (heartbeat timeout stops all actuator endpoints) and the end-to-end distributed verification of all three.

## I4-006 normative rules (architect ruling)

SR-003: when no liveness has been seen for `link.heartbeat_timeout_ms` (the comparison is `>=`), the Edge ends the session as one transition and stops every RUNNING actuator endpoint through the existing `Endpoint::stop()`; sensors are untouched and no endpoint is faulted by the loss of the link. Liveness is refreshed only by a newly admitted, currently valid frame: a duplicate WRITE answered from the ledger, a stale, malformed or wrong-session frame and a rejected HELLO never refresh it. Invariant: no amount of duplicate or replayed WRITE traffic may prevent an otherwise healthy Edge session from reaching the heartbeat timeout and stopping its running actuators once fresh valid traffic has ceased.
