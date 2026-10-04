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
