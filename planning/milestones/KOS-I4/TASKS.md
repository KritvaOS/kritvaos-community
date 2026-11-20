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

**Safety flag (I4-004 to I4-008):** actuator/safety-relevant path. Software acceptance may complete on evidence. The human safety review is an independent gate; it is now CLOSED by the owner's review (kritva-program gate record C015). The KOS-I3 human safety review is a separate gate (C014) that I4 does not close or inherit.

**I4-001 gate:** I4-002 does not start until `docs/architecture/KOS-I4_PROTOCOL.md` and the I4-001 contract headers are review-clean (architect review).

**Commit format:** `KOS-I4 I4-00N <imperative description>`; the `Co-Authored-By` trailer was omitted through I4-007 (cd51be8) and is used from I4-008 onward, as authorised by the user.

## Status

| Task | Status |
|---|---|
| I4-001 | Done; architect review APPROVED (d2f8936) |
| I4-002 | Done (software); architect PASS (52218f4) |
| I4-003 | Done (software); architect PASS (c40fbb2) |
| I4-004 | Done (software); architect PASS (3c535b6); human safety review CLOSED (C015) |
| I4-005 | Done (software); architect PASS (a690520); human safety review CLOSED (C015) |
| I4-006 | Done (software); architect PASS (94aa01d); human safety review CLOSED (C015) |
| I4-007 | Done (software); architect PASS (cd51be8); human safety review CLOSED (C015) |
| I4-008 | Done (software); architect PASS (79f4daf); human safety review CLOSED (C015) |

**KOS-I4 software implementation complete.** The human safety review is CLOSED (C015), as is the independent KOS-I3 review (C014). KOS-CI-001 (Release/ASan/UBSan CI) is CLOSED (kritva-program gate record C011).
