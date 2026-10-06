# SR Requirements

See `docs/requirements/KOS-I4/REQUIREMENTS.md` for the normative traceability table.

Family: SR

## Clarifications (planning review)

Edge-authoritative actuator validation (limits, state, sequence) before every apply; a session-scoped sequence ledger (`last_applied_sequence` and the cached last response per actuator endpoint); a retransmitted identical write returns the cached response and is not applied again; a lower sequence is stale and rejected; a write from an old session id is rejected; heartbeat timeout stops ALL actuator endpoints served in that session through the existing `Endpoint::stop()` (zero/fail-safe motor); no automatic recovery; a new session starts a new ledger. The human safety review is an independent OPEN gate.

## I4-004 / I4-006 split (architect ruling)

The mechanisms of SR-001 (the endpoint validates limits and state before every apply) and SR-002 (session-scoped last-applied sequence and cached response, stale and reordered writes dropped, an accepted HELLO resetting the ledgers and stopping the previous session's running actuators) are implemented in I4-004 because the Edge write path cannot exist safely without them. I4-006 owns SR-003 (heartbeat timeout stops all actuator endpoints) and the end-to-end distributed verification of all three.
