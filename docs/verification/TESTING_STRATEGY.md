# KritvaOS Testing Strategy

## Test Levels

1. **Unit Test** — isolated implementation behavior.
2. **Component Test** — behavior of a runtime component with its immediate dependencies.
3. **Integration Test** — interaction among multiple runtime elements.
4. **System Test** — complete application/runtime behavior.
5. **Sanity Test** — minimal proof that the changed feature builds, launches, executes expected behavior, and exits correctly.
6. **Regression Test** — proof that previously accepted functionality remains working.

## Mandatory Task Gate

Every implementation task shall identify applicable unit, integration, sanity, and regression tests.

```text
Implementation
  ↓
Unit Tests
  ↓
Integration Tests
  ↓
Sanity Test
  ↓
Regression Test
  ↓
Review
  ↓
Commit
```

A failure in a mandatory test blocks task acceptance unless explicitly reviewed and documented.

## Evidence

Each task completion report shall record commands executed, results, test counts, failures, and final PASS/FAIL status.


## KOS-I3 Test Expectations

For Device/Endpoint work, the mandatory task gate additionally covers:

1. Device/Endpoint contract tests.
2. Registry tests.
3. Lifecycle ordering tests.
4. Typed sensor/actuator operation tests.
5. Mock nominal/failure tests.
6. Runtime + Device/Endpoint integration tests.
7. Reference Device/Endpoint sanity/system test.
8. Complete KOS-I2 regression.

Hardware-dependent testing is not an I3 acceptance requirement. I3 tests must run without physical hardware.
