# I3-005 Acceptance Criteria

## Functional
- [ ] Mock Device implemented.
- [ ] Mock IMU endpoint implemented.
- [ ] Mock encoder endpoint implemented.
- [ ] Mock motor endpoint implemented.
- [ ] Nominal sensor data is deterministic.
- [ ] Actuator writes are accepted deterministically.
- [ ] Read failure injection works.
- [ ] Write failure injection works.
- [ ] Health degradation/fault injection works.
- [ ] Operation counters/statistics are maintained.

## Unit
- [ ] Nominal read tests PASS.
- [ ] Nominal write tests PASS.
- [ ] Read-fault tests PASS.
- [ ] Write-fault tests PASS.
- [ ] Statistics tests PASS.
- [ ] Repeated-run determinism test PASS.

## Integration
- [ ] Mock devices operate through registry/runtime contracts.

## Sanity
- [ ] Clean build PASS.
- [ ] Mock device scenario PASS without physical hardware.

## Regression
- [ ] Complete KOS-I2 suite PASS.

## Review
- [ ] Mocks contain no transport/driver dependencies.
- [ ] Mock APIs do not dictate future physical implementation.
- [ ] Ownership is deterministic.

## Git
- [ ] Diff reviewed.
- [ ] Atomic commit.
- [ ] Commit hash recorded after commit.
