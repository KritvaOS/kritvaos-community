# I3-005 Acceptance Criteria

## Functional
- [x] Mock Device implemented.
- [x] Mock IMU endpoints (acceleration, angular velocity) implemented.
- [x] Mock position endpoint implemented (replaces the encoder endpoint; DECISIONS #4).
- [x] Mock motor endpoint implemented.
- [x] Nominal sensor data is deterministic.
- [x] Actuator writes are accepted deterministically.
- [x] Read failure injection works.
- [x] Write failure injection works.
- [x] Health degradation/fault injection works.
- [x] Operation counters/statistics are maintained.

## Unit
- [x] Nominal read tests PASS.
- [x] Nominal write tests PASS.
- [x] Read-fault tests PASS.
- [x] Write-fault tests PASS.
- [x] Statistics tests PASS.
- [x] Repeated-run determinism test PASS.

## Integration
- [x] Mock devices operate through registry/runtime contracts.

## Sanity
- [x] Clean build PASS.
- [x] Mock device scenario PASS without physical hardware.

## Regression
- [x] Complete KOS-I2 suite PASS.

## Review
- [x] Mocks contain no transport/driver dependencies.
- [x] Mock APIs do not dictate future physical implementation.
- [x] Ownership is deterministic.

## Git
- [x] Diff reviewed.
- [x] Atomic commit.
- [ ] Commit hash recorded after commit.

## Evidence (I3-005)

- Build: Debug, 0 warnings from KOS-I3 code; the unit and integration tests also ran clean under ASan+UBSan.
- Unit: `kritva_hardware_mock_unit` PASS (conformance of all four mock endpoints, deterministic nominal data and formulas, identical instances, sequence restart per live period, settings and configuration errors with unchanged previous settings, read failure injection (once, no fault, no data gap), fault injection (after N, listener once, RESOURCE_UNAVAILABLE, explicit restart), degradation, motor validation and inclusive limits, limit configuration, write failure and fault, fail-safe, position following the command, devices and capabilities).
- Integration: `kritva_hardware_mock_integration` PASS (config keys cover the mocks; typo rejected; configured run; scheduled fault observed through the unchanged runtime with exactly one ERROR event; disabled device; determinism).
- Sanity: `kritva_hardware_mock_sanity` PASS (exact deterministic output); `kritva_hardware_mock_header_sanity` PASS; configure-time check that `kritva_hardware_mock` links only `kritva::hardware`.
- Regression: `ctest` 110/110 PASS. `make check` PASS.
- Mutation checks (no command validation, fail-safe on stop, fail-safe effective velocity, fail-after off by one, sequence reset) each fail the tests.
- Core: unchanged. Mocks are a reusable library (`hardware/mock/`, DECISIONS #7) and use no driver, bus, transport or wall clock.
- **HUMAN REVIEW REQUIRED (AGENTS.md section 32, physical actuator control):** the motor mock's behavior as the reference for the actuator contract: validate-before-apply, rejected and failed writes not applied, velocity forced to zero at initialize/stop/shutdown, a non-RUNNING (stopped or faulted) motor has zero effective velocity. Not yet reviewed by a human.
