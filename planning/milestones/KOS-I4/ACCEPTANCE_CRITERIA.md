# KOS-I4 Acceptance Criteria

- [x] All I4 requirements accepted with evidence.
- [x] HELLO/version negotiation and discovery pass deterministically.
- [x] Configure/lifecycle/read/write/status/health/statistics/heartbeat pass end-to-end.
- [x] Edge validates actuator limits before apply and never applies a duplicate/replay.
- [x] Heartbeat timeout stops affected Edge actuators; Nexus remote endpoint faults once.
- [x] Stale/late/duplicate/malformed messages are rejected deterministically.
- [x] No automatic recovery; explicit shutdown->initialize->start creates a fresh session.
- [x] Full I2/I3 regression, ASan, UBSan and mutation/fault injection pass.
- [ ] HUMAN SAFETY REVIEW — OPEN. This is an independent human-owned gate. It is not completed by software implementation or automated verification. It blocks formal KOS-I4 closure and physical actuator deployment, but does not block subsequent software tasks.
- [ ] I3 human safety review remains OPEN independently.

## Closure record (software)

All eight tasks are architect-accepted: I4-001 d2f8936, I4-002 52218f4, I4-003 c40fbb2, I4-004 3c535b6, I4-005 a690520, I4-006 94aa01d, I4-007 cd51be8, I4-008 79f4daf. Per-task evidence is in `I4-00N/ACCEPTANCE_CRITERIA.md` and `docs/verification/KOS-I4_VERIFICATION.md`. Core R1.0 and the I3 contracts were unchanged throughout. Fresh-clone evidence: the GitHub `ci.yml` run (clean checkout with submodules) passed on every I4 commit.

**Software implementation: COMPLETE.** The two unchecked items above are separate human-owned gates and are not closed by this record: the KOS-I4 human safety review and the independent KOS-I3 human safety review. Physical actuator deployment remains blocked by them.

**Deferred:** KOS-CI-001 (Release, ASan and UBSan CI jobs) is a separate task and remains deferred; those results are local evidence only.
