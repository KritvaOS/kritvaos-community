# I3-001 Acceptance Criteria

## Functional
- [ ] Device identity contract defined.
- [ ] Endpoint identity contract defined.
- [ ] Device-to-Endpoint ownership relationship defined.
- [ ] Capability contract defined.
- [ ] Lifecycle contract defined.
- [ ] Status/health/error semantics defined.
- [ ] Sensor and actuator specialization boundary defined.
- [ ] Scope limited to I3-001.

## Unit
- [ ] Contract tests cover valid identities.
- [ ] Contract tests cover invalid identities.
- [ ] Capability behavior tested.
- [ ] Unit tests PASS.

## Integration
- [ ] Applicable runtime integration contract test PASS.

## Sanity
- [ ] Clean build PASS.
- [ ] Public headers compile in the supported build.
- [ ] No physical driver or transport dependency.

## Regression
- [ ] KOS-I2 tests PASS.
- [ ] No unexpected regression.

## Review
- [ ] Core R1.0 boundary respected.
- [ ] No duplicate Core types.
- [ ] No competing lifecycle engine.
- [ ] No universal sensor/actuator variant introduced.
- [ ] I3/I4 boundary documented.

## Git
- [ ] Diff reviewed.
- [ ] Atomic commit.
- [ ] Commit hash recorded after commit.
