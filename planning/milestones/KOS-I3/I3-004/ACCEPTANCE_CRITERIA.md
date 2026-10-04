# I3-004 Acceptance Criteria

## Functional
- [ ] Typed sensor read contract defined.
- [ ] Typed actuator write contract defined.
- [ ] `ImuSample` contract defined.
- [ ] `EncoderSample` contract defined.
- [ ] `MotorCommand` contract defined.
- [ ] Units and validity semantics documented.
- [ ] Operation-before-ready behavior defined.
- [ ] Operation-after-stop behavior defined.
- [ ] Failure result semantics defined.
- [ ] No universal sensor/actuator data variant introduced.

## Unit
- [ ] Sample construction/default tests PASS.
- [ ] Validity/units tests PASS.
- [ ] Read/write contract tests PASS.
- [ ] Error-path tests PASS.

## Integration
- [ ] Typed endpoints compile and operate through the Device contract.

## Sanity
- [ ] Clean build PASS.
- [ ] Deterministic sample/command scenario PASS.

## Regression
- [ ] Complete KOS-I2 suite PASS.

## Review
- [ ] Data contracts are minimal.
- [ ] No real-time guarantee introduced.
- [ ] No transport-specific data structure introduced.

## Git
- [ ] Diff reviewed.
- [ ] Atomic commit.
- [ ] Commit hash recorded after commit.
