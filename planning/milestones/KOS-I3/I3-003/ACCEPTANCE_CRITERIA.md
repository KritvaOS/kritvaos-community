# I3-003 Acceptance Criteria

## Functional
- [ ] Device/Endpoint lifecycle integration implemented.
- [ ] Initialization ordering is deterministic.
- [ ] Start ordering is deterministic.
- [ ] Stop ordering is deterministic.
- [ ] Invalid lifecycle operations fail deterministically.
- [ ] Endpoint failure is visible to runtime observation.
- [ ] Runtime remains able to perform controlled shutdown after endpoint failure.
- [ ] No second global lifecycle state machine exists.

## Unit
- [ ] Lifecycle unit tests PASS.
- [ ] Invalid transition tests PASS.
- [ ] Failure-path tests PASS.

## Integration
- [ ] Runtime + registry + endpoint lifecycle test PASS.
- [ ] Shutdown-after-failure test PASS.

## Sanity
- [ ] Clean build PASS.
- [ ] Lifecycle sequence executes end-to-end.

## Regression
- [ ] Complete KOS-I2 suite PASS.

## Review
- [ ] Core lifecycle/status contracts reused.
- [ ] Runtime ownership is clear.
- [ ] No Core source/API changes.

## Git
- [ ] Diff reviewed.
- [ ] Atomic commit.
- [ ] Commit hash recorded after commit.
