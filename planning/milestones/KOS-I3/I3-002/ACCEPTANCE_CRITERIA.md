# I3-002 Acceptance Criteria

## Functional
- [ ] Device registry implemented.
- [ ] Device registration implemented.
- [ ] Device lookup implemented.
- [ ] Endpoint registration implemented.
- [ ] Endpoint lookup implemented.
- [ ] Duplicate Device IDs rejected.
- [ ] Duplicate Endpoint IDs rejected within a Device.
- [ ] Missing Device/Endpoint lookup fails deterministically.
- [ ] Enumeration order is deterministic.
- [ ] Registry ownership/lifetime is documented.

## Unit
- [ ] Registration tests PASS.
- [ ] Duplicate tests PASS.
- [ ] Lookup tests PASS.
- [ ] Missing lookup tests PASS.
- [ ] Enumeration tests PASS.

## Integration
- [ ] Runtime can discover registered devices/endpoints.

## Sanity
- [ ] Clean build PASS.
- [ ] Basic register/discover scenario PASS.

## Regression
- [ ] All KOS-I2 tests PASS.
- [ ] No unexpected regression.

## Review
- [ ] Registry is separate from Core RuntimeManager registry.
- [ ] No speculative service-discovery framework.
- [ ] Error semantics use existing Core/result conventions where applicable.

## Git
- [ ] Diff reviewed.
- [ ] Atomic commit.
- [ ] Commit hash recorded after commit.
