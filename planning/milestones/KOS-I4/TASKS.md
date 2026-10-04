# KOS-I4 Tasks

| Task | Name | Dependency | Safety |
|---|---|---|---|
| I4-001 | Architecture, Node Identity & Protocol Contract | I3 | — |
| I4-002 | Frame Format & Protocol Codec | I4-001 | — |
| I4-003 | Deterministic Simulated Transport | I4-002 | — |
| I4-004 | EdgeHost & Remote Service | I4-003 | write path present |
| I4-005 | RemoteDevice & RemoteEndpoint | I4-004 | HUMAN SAFETY REVIEW REQUIRED — actuator write |
| I4-006 | Distributed Failure & Actuator Safety | I4-005 | HUMAN SAFETY REVIEW REQUIRED |
| I4-007 | Runtime Integration & Diagnostics | I4-006 | HUMAN SAFETY REVIEW REQUIRED — actuator write |
| I4-008 | Nexus↔Edge Reference Demo | I4-007 | HUMAN SAFETY REVIEW REQUIRED — actuator write |

Strictly sequential; one atomic commit per task.
