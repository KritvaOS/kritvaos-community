# KOS-I4 Acceptance Criteria

- [x] All I4 requirements accepted with evidence.
- [x] HELLO/version negotiation and discovery pass deterministically.
- [x] Configure/lifecycle/read/write/status/health/statistics/heartbeat pass end-to-end.
- [x] Edge validates actuator limits before apply and never applies a duplicate/replay.
- [x] Heartbeat timeout stops affected Edge actuators; Nexus remote endpoint faults once.
- [x] Stale/late/duplicate/malformed messages are rejected deterministically.
- [x] No automatic recovery; explicit shutdown->initialize->start creates a fresh session.
- [x] Full I2/I3 regression, ASan, UBSan and mutation/fault injection pass.
- [x] HUMAN SAFETY REVIEW — CLOSED. This is an independent human-owned gate, closed by the owner's review and not by software implementation or automated verification. Recorded in the program ledger (kritva-program gate record C015, Community commit b62cc2e, 2026-11-20; earlier closure C010 at 8256ba3). It covers the software contract only and does not approve a physical actuator deployment.
- [x] I3 human safety review: CLOSED independently (kritva-program gate record C014).

## Closure record (software)

All eight tasks are architect-accepted: I4-001 d2f8936, I4-002 52218f4, I4-003 c40fbb2, I4-004 3c535b6, I4-005 a690520, I4-006 94aa01d, I4-007 cd51be8, I4-008 79f4daf. Per-task evidence is in `I4-00N/ACCEPTANCE_CRITERIA.md` and `docs/verification/KOS-I4_VERIFICATION.md`. Core R1.0 and the I3 contracts were unchanged throughout. Fresh-clone evidence: the GitHub `ci.yml` run (clean checkout with submodules) passed on every I4 commit.

**Software implementation: COMPLETE.** The two safety items above are separate human-owned gates, closed by the owner's review (C015 and C014), not by this record. Physical actuator deployment still needs the integrator steps in `docs/architecture/hardware/DRIVER_SAFETY_CONTRACT.md`.

**KOS-CI-001** (Release, ASan and UBSan CI jobs) is CLOSED (kritva-program gate record C011): the three variants ran green in CI on 8256ba3 (Community run #34).
