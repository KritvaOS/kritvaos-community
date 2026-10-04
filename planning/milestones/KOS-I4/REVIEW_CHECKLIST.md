# KOS-I4 Review Checklist

- [ ] I3 Endpoint contract unchanged.
- [ ] Core R1.0 unchanged.
- [ ] Existing DeviceManager reused; no second manager.
- [ ] Session state isolated from Core lifecycle.
- [ ] Bounds/version/length/string checks occur before use.
- [ ] No raw C++ struct serialization.
- [ ] Stale/duplicate/unknown messages rejected.
- [ ] Request deadlines use virtual time only.
- [ ] No automatic recovery.
- [ ] Edge is safety authority; limit validation is before apply.
- [ ] Session-scoped replay ledger prevents duplicate/replay application.
- [ ] Heartbeat loss stops affected actuators.
- [ ] HUMAN SAFETY REVIEW REQUIRED for I4-006 and every actuator write path.
- [ ] I3 safety review remains OPEN independently.
- [ ] Unit/integration/regression/ASan/UBSan/mutation/fresh-clone evidence complete.
