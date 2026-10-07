# DR Requirements

See `docs/requirements/KOS-I4/REQUIREMENTS.md` for the normative traceability table.

Family: DR

## I4-007 normative rules (architect ruling)

DR-001..003: diagnostics are read-only snapshots (`diagnose`) and deterministic sanitized text (`describe`) of the Nexus side (`RemoteNode`) and, separately, of the Edge (`EdgeHost`). Calling any passive diagnostic API SHALL NOT refresh liveness, heartbeat supervision, sequence watermarks, deadlines, session state or transport state, and SHALL NOT reconnect, restart, clear a FAULT, reset a statistic or acknowledge a safety condition. The link-loss records are exposed as copies only. The fault origin of an episode (LINK_LOST, SESSION_CLOSED, REMOTE_FAULT, LOCAL_FAILURE) is recorded explicitly and never changes within the episode; the reasons `link lost`, `session closed` and `remote fault: <reason>` are never collapsed into a generic remote error. `RemoteNode` holds no reference to an `EdgeHost`. Active observation of the Edge over the link is deferred to a later task.

## Clarification of what diagnostics may show (I4-007 review, carried to I4-008)

Diagnostics SHALL NOT expose arbitrary application or customer configuration, settings sent to endpoints, or secrets. The negotiated link timing (heartbeat period and timeout, request timeout) is protocol and session state, derived from the configuration but part of what a session is, and is legitimately shown by `describe()`. "No configuration value" means no application configuration value.
