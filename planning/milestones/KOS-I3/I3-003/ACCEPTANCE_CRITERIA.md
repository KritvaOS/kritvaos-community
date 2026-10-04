# I3-003 Acceptance Criteria

## Functional
- [x] Device/Endpoint lifecycle integration implemented.
- [x] Initialization ordering is deterministic.
- [x] Start ordering is deterministic.
- [x] Stop ordering is deterministic.
- [x] Invalid lifecycle operations fail deterministically.
- [x] Endpoint failure is visible to runtime observation.
- [x] Runtime remains able to perform controlled shutdown after endpoint failure.
- [x] No second global lifecycle state machine exists.

## Unit
- [x] Lifecycle unit tests PASS.
- [x] Invalid transition tests PASS.
- [x] Failure-path tests PASS.

## Integration
- [x] Runtime + registry + endpoint lifecycle test PASS.
- [x] Shutdown-after-failure test PASS.

## Sanity
- [x] Clean build PASS.
- [x] Lifecycle sequence executes end-to-end.

## Regression
- [x] Complete KOS-I2 suite PASS.

## Review
- [x] Core lifecycle/status contracts reused.
- [x] Runtime ownership is clear.
- [x] No Core source/API changes.

## Git
- [x] Diff reviewed.
- [x] Atomic commit.
- [ ] Commit hash recorded after commit.

## Evidence (I3-003)

- Build: Debug, 0 warnings from KOS-I3 code; the unit test also ran clean under ASan+UBSan (it found and fixed a dangling temporary in `DeviceManager::capabilities()`).
- Unit: `kritva_hardware_manager_unit` PASS (identity, exact stop/shutdown reversal, registry closing, invalid operations with source, config keys and scoped settings, configuration errors, disabled device, failing initialize/start/stop hooks, endpoint fault while RUNNING with exactly one ERROR event, no silent recovery, stop skips faulted, shutdown releases, explicit restart, statistics, capabilities, listener cleanup on destruction). The Endpoint conformance suite gained setting-name and fault-listener checks.
- Integration: `kritva_hardware_manager_integration` PASS (DeviceManager as one Component in the unchanged RuntimeHost with a dependency; allow-list rejects typos; endpoint failure visible in observe()/failure_report(); initialize failure through run(); explicit restart).
- Sanity: `kritva_hardware_manager_sanity` PASS (READY, RUNNING, endpoint FAULT while runtime stays RUNNING, failure_report, controlled shutdown).
- Regression: `ctest` 104/104 PASS (76 Core + 18 I2 + 10 I3). `make check` PASS.
- Mutation checks (event during lifecycle, shutdown without stop, order, no fault listener) each fail the tests.
- Core and RuntimeHost: unchanged. Additive Endpoint contract amendments: `setting_names()` and a fault listener (called once per fault).
