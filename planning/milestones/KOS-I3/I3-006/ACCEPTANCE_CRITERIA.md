# I3-006 Acceptance Criteria

## Functional
- [x] Device status observable.
- [x] Endpoint status observable.
- [x] Device health observable.
- [x] Endpoint health observable.
- [x] Capabilities observable.
- [x] Operation statistics observable.
- [x] Last failure/result is observable.
- [x] Fault injection produces deterministic failure evidence.
- [x] Faulted endpoint does not silently recover.
- [x] Controlled shutdown remains possible after endpoint failure.

## Unit
- [x] Observation tests PASS.
- [x] Statistics tests PASS.
- [x] Fault reporting tests PASS.
- [x] No-silent-recovery test PASS.

## Integration
- [x] Runtime observation includes device/endpoint failure information.
- [x] Controlled shutdown after endpoint fault PASS.

## Sanity
- [x] Clean build PASS.
- [x] Fault scenario produces expected observable output.

## Regression
- [x] Complete KOS-I2 suite PASS.

## Review
- [x] Existing Core/runtime diagnostics reused where applicable.
- [x] No sensitive information exposed.
- [x] No automatic recovery policy added.

## Git
- [x] Diff reviewed.
- [x] Atomic commit.
- [x] Commit hash recorded: `46b91fa`.

## Evidence (I3-006)

- Build: Debug, 0 warnings from KOS-I3 code; the new unit and integration tests also ran clean under ASan+UBSan.
- Unit: `kritva_hardware_diagnostics_unit` PASS (snapshot before use, running snapshot with statistics/last error/capabilities, fault evidence and failure reason, refused reads counted with no recovery, disabled device marked, exact and deterministic text, messages cannot forge lines, no configuration values in diagnostics).
- Integration: `kritva_hardware_fault_integration` PASS (scheduled endpoint fault through the DeviceManager and the unchanged RuntimeHost: device text evidence, I2 `describe()` shows the manager UNHEALTHY with the endpoint reason, exactly one ERROR event, controlled shutdown, and a second identical run gives identical text and events).
- Sanity: `kritva_hardware_diagnostics_sanity` PASS (running, faulted and shutdown diagnostics printed).
- Regression: `ctest` 113/113 PASS. `make check` PASS.
- Mutation checks (control-character sanitising, failed-operation counter, last error) each fail the tests.
- Core and RuntimeHost: unchanged. Reuses the I2 observation (ComponentObservation, IComponentStatistics, EventLog); no second diagnostics framework.
- Fault policy confirmed by tests: a faulted endpoint never recovers by itself; the explicit path is stop/shutdown then initialize (covered in I3-003 and I3-005); no automatic recovery exists (DER-705).
