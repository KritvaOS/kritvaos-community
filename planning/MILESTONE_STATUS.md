# Milestone Status

| Milestone | Status |
|---|---|
| KOS-I2 | IMPLEMENTED / predecessor baseline |
| KOS-I3 | IMPLEMENTED; audit and re-audit remediation complete; human safety review CLOSED (kritva-program gate record C014, Community commit b62cc2e, 2026-11-20) |
| KOS-I4 | SOFTWARE IMPLEMENTATION COMPLETE (I4-001..I4-008, architect-accepted); human safety review CLOSED (kritva-program gate record C015, Community commit b62cc2e, 2026-11-20); KOS-CI-001 CLOSED (kritva-program gate record C011) |
| KOS-M0 | V2-M0 governance task KOS-COMMUNITY-M0-001 DESCOPED (owner waived the PD-017 mailbox test, program D-119; removed from baseline v12; not tested, no credit) |

## KOS-I2 Task Commits

| Task | Commit |
|---|---|
| I2-001 | 150d211 |
| I2-002 | e2851fd |
| I2-003 | ae0de75 |
| I2-004 | bfa2b29 |
| I2-005 | e8ad793 |
| I2-006 | 69521ef |

## KOS-I3 Task Commits

| Task | Commit |
|---|---|
| I3-001 | 39780cc |
| I3-002 | a7e08e5 |
| I3-003 | 97ab518 |
| I3-004 | f980675 |
| I3-005 | cd1a35c |
| I3-006 | 46b91fa |
| I3-007 | d8cd315 |
| Audit remediation (one follow-up commit; the historical task commits are not rewritten) | 40a6940 |
| Re-audit follow-up (minor items N1, N2, N3, N5) | 9912e3c |
| Planning closure | e61814e (plan, task records), ecc0ca3 (milestone acceptance), the review-checklist closure commit |

## KOS-I3 closure gates

| Gate | Status |
|---|---|
| Technical acceptance (7 tasks) | PASS |
| Milestone acceptance record (`planning/milestones/KOS-I3/ACCEPTANCE_CRITERIA.md`) | PASS: all software items checked with evidence; the safety gate is listed separately and left unchecked |
| Independent audit and remediation | PASS: all findings fixed; targeted re-audit found no blockers or majors; its minor items are fixed |
| Documentation closure (plan aligned to the as-built architecture, commit hashes recorded, execution order, milestone acceptance, review checklist) | PASS (e61814e, ecc0ca3 and the review-checklist closure commit) |
| CI | The `build-and-test` job is implemented and its commands were verified locally and in the project container; an external GitHub run for the final commit was not independently confirmed in the external review |
| Human safety review: `MotorCommand`, motor limits, fault-to-zero behavior, velocity command semantics, mock safety behavior, any future mapping to physical actuator control | **CLOSED**: owner review recorded in the program ledger (kritva-program gate record C014, Community commit b62cc2e, 2026-11-20; earlier closure C009 at 8256ba3). Covers the software contract only |

KOS-I3's safety gate is closed by the owner's review. That closure does not approve a physical actuator deployment.

## KOS-I4 Task Commits

| Task | Commit |
|---|---|
| I4-001 | d2f8936 |
| I4-002 | 52218f4 (after 15c1781) |
| I4-003 | c40fbb2 |
| I4-004 | 3c535b6 |
| I4-005 | a690520 |
| I4-006 | 94aa01d |
| I4-007 | cd51be8 |
| I4-008 | 79f4daf |

## KOS-I4 closure gates

| Gate | Status |
|---|---|
| Software acceptance (8 tasks, architect review) | PASS |
| Milestone acceptance record (`planning/milestones/KOS-I4/ACCEPTANCE_CRITERIA.md`) | Software items checked with evidence; the safety gate is listed separately and left unchecked |
| CI | `ci.yml` (Debug build and full test suite) and the source-header check pass on every I4 commit; the Release, ASan and UBSan variants ran green in CI on 8256ba3 (Community run #34); **KOS-CI-001 CLOSED** (kritva-program gate record C011) |
| Human safety review: the I4 actuator write paths (Edge limit validation, duplicate/replay protection, heartbeat-timeout stop, fault-to-safe-state) | **CLOSED**: owner review recorded in the program ledger (kritva-program gate record C015, Community commit b62cc2e, 2026-11-20; earlier closure C010 at 8256ba3). Covers the software contract only |

Both safety gates (KOS-I3 and KOS-I4) are closed by the owner's review. A closure does not approve a physical actuator deployment: see `docs/architecture/hardware/DRIVER_SAFETY_CONTRACT.md`.
