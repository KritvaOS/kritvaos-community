# KOS-I4 Tasks

| Task | Name | Dependency | Safety |
|---|---|---|---|
| I4-001 | Architecture, Node Identity & Protocol Contract | I3 | — |
| I4-002 | Frame Format & Protocol Codec | I4-001 | — |
| I4-003 | Deterministic Simulated Transport | I4-002 | — |
| I4-004 | EdgeHost & Remote Service | I4-003 | Safety-relevant (write path present) |
| I4-005 | RemoteDevice & RemoteEndpoint | I4-004 | Safety-relevant (actuator write path) |
| I4-006 | Distributed Failure & Actuator Safety | I4-005 | Safety-relevant |
| I4-007 | Runtime Integration & Diagnostics | I4-006 | Safety-relevant (actuator write path) |
| I4-008 | Nexus↔Edge Reference Demo | I4-007 | Safety-relevant (actuator write path) |

Strictly sequential; one atomic commit per task.

**Safety flag (I4-004 to I4-008):** actuator/safety-relevant path. Software acceptance may complete on evidence. Human safety review remains an independent OPEN gate and is required before formal KOS-I4 closure or physical actuator deployment; it does not block the next software task. The KOS-I3 human safety review is a separate OPEN gate that I4 must not close or inherit.

**I4-001 gate:** I4-002 does not start until `docs/architecture/KOS-I4_PROTOCOL.md` and the I4-001 contract headers are review-clean (architect review).

**Commit format:** `KOS-I4 I4-00N <imperative description>`; no `Co-Authored-By` trailer.

## Status

| Task | Status |
|---|---|
| I4-001 | Done; architect review APPROVED (d2f8936) |
| I4-002 | Done (software); landed commit under architect review |
| I4-003 | Done (software); landed commit under architect review |
| I4-004 | Done (software); landed commit under architect review; HUMAN SAFETY REVIEW OPEN |
| I4-005 | Done (software); landed commit under architect review; HUMAN SAFETY REVIEW OPEN |
| I4-006 | Done (software); landed commit under architect review; HUMAN SAFETY REVIEW OPEN |
| I4-007 | Done (software); landed commit under architect review; HUMAN SAFETY REVIEW OPEN |
| I4-008 | Planned |
