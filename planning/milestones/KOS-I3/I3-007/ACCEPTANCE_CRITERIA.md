# I3-007 Acceptance Criteria

## Functional
- [x] Reference device demo exists.
- [x] IMU device and endpoints are registered.
- [x] Motor device and endpoints are registered.
- [x] Endpoint discovery is demonstrated.
- [x] IMU read is demonstrated.
- [x] Motor command write is demonstrated.
- [x] Status/health/statistics are demonstrated.
- [x] Endpoint fault is injected.
- [x] Failure is observed.
- [x] Controlled shutdown completes.
- [x] Demo exits deterministically.

## Unit
- [x] Demo tests PASS: there are no separate unit tests of the demo support code; the demo is covered by in-process system tests (`kritva_device_demo_system`) and the end-to-end executable sanity test (`kritva_device_demo_sanity`), and its building blocks by the I3-001..006 unit and integration tests.

## Integration
- [x] Full runtime + registry + mock devices + endpoints test PASS.

## Sanity
- [x] Clean checkout build PASS.
- [x] Demo launch PASS.
- [x] Expected output/flow PASS.
- [x] Clean exit PASS.

## Regression
- [x] Complete KOS-I2 suite PASS.
- [x] Complete I3 suite PASS.

## Review
- [x] Demo uses only approved I3 contracts.
- [x] No hardware/transport dependency.
- [x] Documentation matches implementation.

## Git
- [x] Diff reviewed.
- [x] Atomic commit.
- [x] Commit hash recorded: `d8cd315`.

## Evidence (I3-007)

- Build: clean `rm -rf build`, Debug and Release presets: 0 warnings from KOS-I3/KOS-I2 code (Core's own `component_context_test` has one GCC 13 `-Wpessimizing-move` warning; Core is unchanged).
- System (in-process): `kritva_device_demo_system` PASS (fault scenario in the demo-plan order; clean scenario; invalid configurations never reach READY; unusable command and disabled device end in a controlled error; configured device settings active; a scheduled mock fault ends in a controlled error; allow-list rejects typos; identical output on repeated runs and no state leaks).
- Sanity / end-to-end: `kritva_device_demo_sanity` PASS (executable: fault scenario exit 0, clean scenario exit 0, invalid configuration exit 1, misspelled key exit 1 and reported, default run exit 0).
- Regression: `ctest` 115/115 PASS in Debug and Release (76 Core + 18 I2 + 21 I3). `make check` PASS. The new I3 tests also ran clean under ASan+UBSan.
- Mutation checks: removing the fault injection fails the tests. The demo's own defensive checks (no-recovery, error-event count) are runtime assertions that only fire if the library misbehaves; removing them is not detectable by demo tests (the library behavior is covered by the I3-003/005/006 tests).
- Core: unchanged.
- Demo outcome: exit code 0 when the scenario behaves as expected (fault or clean), 1 on any error or unexpected result (the demo plan expects a successful exit after the controlled fault and shutdown).
