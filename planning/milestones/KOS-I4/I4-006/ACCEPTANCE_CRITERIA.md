# I4-006 Acceptance Criteria

- [ ] All requirements traced to I4-006 are implemented and evidenced.
- [ ] I3 contracts remain unchanged.
- [ ] Debug and Release builds pass.
- [ ] Unit/integration/sanity tests pass.
- [ ] Full KOS-I2 regression passes.
- [ ] Full KOS-I3 regression passes.
- [ ] ASan passes.
- [ ] UBSan passes.
- [ ] Applicable mutation/fault-injection checks pass.
- [ ] HUMAN SAFETY REVIEW — OPEN. This is an independent human-owned gate. It is not completed by software implementation or automated verification. It blocks formal KOS-I4 closure and physical actuator deployment, but does not block subsequent software tasks.
- [ ] Review checklist/evidence is complete.
- [ ] One atomic commit SHA is recorded.
- [ ] Core R1.0 source is unchanged.

## Carry-forward from the I4-004 architect review

- [ ] The heartbeat campaign decides and tests exactly which frames refresh the Edge liveness timestamp (protocol section 11: any valid in-session frame): a normal admitted frame, an admitted duplicate WRITE (answered from the ledger by `EdgeHost::handle_write` without calling `admit()`, so it does not refresh `last_valid_frame_ns` today), a stale WRITE, a malformed frame, a wrong-session frame and a heartbeat. I4-004 was not reopened for this; I4-006 must either change the duplicate-WRITE path or document and test the chosen interpretation.
