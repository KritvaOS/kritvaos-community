# SR Requirements

See `docs/requirements/KOS-I4/REQUIREMENTS.md` for the normative traceability table.

Family: SR

## Clarifications (planning review)

Edge-authoritative actuator validation (limits, state, sequence) before every apply; a session-scoped sequence ledger (`last_applied_sequence` and the cached last response per actuator endpoint); a retransmitted identical write returns the cached response and is not applied again; a lower sequence is stale and rejected; a write from an old session id is rejected; heartbeat timeout stops ALL actuator endpoints served in that session through the existing `Endpoint::stop()` (zero/fail-safe motor); no automatic recovery; a new session starts a new ledger. The human safety review is an independent OPEN gate.
