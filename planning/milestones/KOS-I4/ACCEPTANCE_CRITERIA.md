# KOS-I4 Acceptance Criteria

- [ ] All I4 requirements accepted with evidence.
- [ ] HELLO/version negotiation and discovery pass deterministically.
- [ ] Configure/lifecycle/read/write/status/health/statistics/heartbeat pass end-to-end.
- [ ] Edge validates actuator limits before apply and never applies a duplicate/replay.
- [ ] Heartbeat timeout stops affected Edge actuators; Nexus remote endpoint faults once.
- [ ] Stale/late/duplicate/malformed messages are rejected deterministically.
- [ ] No automatic recovery; explicit shutdown->initialize->start creates a fresh session.
- [ ] Full I2/I3 regression, ASan, UBSan and mutation/fault injection pass.
- [ ] HUMAN SAFETY REVIEW REQUIRED for I4-006 and every actuator write path.
- [ ] I3 human safety review remains OPEN independently.
