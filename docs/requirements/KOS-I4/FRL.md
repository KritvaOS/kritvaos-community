# FRL Requirements

See `docs/requirements/KOS-I4/REQUIREMENTS.md` for the normative traceability table.

Family: FRL

## Clarifications (planning review)

Exactly one ERROR event per endpoint fault transition (FRL-001); a link-level failure affecting N remote endpoints produces N endpoint ERROR events plus one link-level diagnostic record, which does not replace the endpoint events. No duplicate ERROR for an already-faulted endpoint. A request that has timed out can never be satisfied by a late response.

## I4-006 normative rules (architect ruling)

FRL-001 (cardinality): for a Nexus link loss with N live remote endpoints (READY or RUNNING): exactly N endpoint FAULT transitions, N endpoint ERROR notifications and one link-level diagnostic record; driving the session again after the loss produces nothing more; an endpoint that is UNKNOWN, STOPPED or already FAULT is not counted. Reasons are `link lost`, `session closed` and `remote fault: <reason>`. FAULT_EVENT reports a FAULT transition only (never FAULT to FAULT). FRL-003: nothing recovers automatically (no reconnect, no restart, no clearing of a FAULT by a fresh HELLO); the way back is shutdown, initialize, start. FRL-002 (Nexus side, I4-005 mechanism, verified here): late, duplicate, unknown-correlation, wrong-session and wrong-type responses are rejected, and none of them counts as liveness.
