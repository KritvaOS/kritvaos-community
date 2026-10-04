# I3-006 Acceptance Criteria

## Functional
- [ ] Device status observable.
- [ ] Endpoint status observable.
- [ ] Device health observable.
- [ ] Endpoint health observable.
- [ ] Capabilities observable.
- [ ] Operation statistics observable.
- [ ] Last failure/result is observable.
- [ ] Fault injection produces deterministic failure evidence.
- [ ] Faulted endpoint does not silently recover.
- [ ] Controlled shutdown remains possible after endpoint failure.

## Unit
- [ ] Observation tests PASS.
- [ ] Statistics tests PASS.
- [ ] Fault reporting tests PASS.
- [ ] No-silent-recovery test PASS.

## Integration
- [ ] Runtime observation includes device/endpoint failure information.
- [ ] Controlled shutdown after endpoint fault PASS.

## Sanity
- [ ] Clean build PASS.
- [ ] Fault scenario produces expected observable output.

## Regression
- [ ] Complete KOS-I2 suite PASS.

## Review
- [ ] Existing Core/runtime diagnostics reused where applicable.
- [ ] No sensitive information exposed.
- [ ] No automatic recovery policy added.

## Git
- [ ] Diff reviewed.
- [ ] Atomic commit.
- [ ] Commit hash recorded after commit.
