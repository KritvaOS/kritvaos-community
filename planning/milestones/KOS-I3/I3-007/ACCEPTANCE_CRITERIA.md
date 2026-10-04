# I3-007 Acceptance Criteria

## Functional
- [ ] Reference device demo exists.
- [ ] IMU device and endpoints are registered.
- [ ] Motor device and endpoints are registered.
- [ ] Endpoint discovery is demonstrated.
- [ ] IMU read is demonstrated.
- [ ] Motor command write is demonstrated.
- [ ] Status/health/statistics are demonstrated.
- [ ] Endpoint fault is injected.
- [ ] Failure is observed.
- [ ] Controlled shutdown completes.
- [ ] Demo exits deterministically.

## Unit
- [ ] Demo support code unit tests PASS.

## Integration
- [ ] Full runtime + registry + mock devices + endpoints test PASS.

## Sanity
- [ ] Clean checkout build PASS.
- [ ] Demo launch PASS.
- [ ] Expected output/flow PASS.
- [ ] Clean exit PASS.

## Regression
- [ ] Complete KOS-I2 suite PASS.
- [ ] Complete I3 suite PASS.

## Review
- [ ] Demo uses only approved I3 contracts.
- [ ] No hardware/transport dependency.
- [ ] Documentation matches implementation.

## Git
- [ ] Diff reviewed.
- [ ] Atomic commit.
- [ ] Commit hash recorded after commit.
