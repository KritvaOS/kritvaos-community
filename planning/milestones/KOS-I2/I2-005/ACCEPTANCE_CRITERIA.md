# I2-005 Acceptance Criteria

## Functional
- [x] Functional requirements for I2-005 implemented.
- [x] Error paths are deterministic.
- [x] Scope remains limited to I2-005.

## Unit Test
- [x] Unit tests added/updated for new logic.
- [x] Unit tests PASS.

## Integration Test
- [x] Applicable integration tests added/updated.
- [x] Integration tests PASS.

## Sanity Test
- [x] Clean build PASS.
- [x] Feature launches/executes as intended.
- [x] Expected observable behavior verified.
- [x] Clean exit/shutdown verified where applicable.

## Regression Test
- [x] Previous KOS-I2 tests PASS.
- [x] Applicable repository tests PASS.
- [x] No unexpected regression.

## Code Review
- [x] Architecture compliance verified.
- [x] Core R1.0 boundary respected.
- [x] No unrelated changes.
- [x] No speculative abstraction.

## Git
- [x] Working tree reviewed.
- [x] Tests PASS at commit point.
- [x] Diff reviewed.
- [x] Commit message follows GIT_COMMIT_STEP.md.
- [ ] Commit hash recorded (in planning/MILESTONE_STATUS.md).

## Evidence (I2-005)

- Build: PASS, 0 warnings.
- Unit: `kritva_failure_handling_unit` PASS (observable failure, propagation report, deterministic/pure report, controlled shutdown after failure, no silent recovery, failure in each step, shutdown from any quiet state, explicit restart, deterministic scenario).
- Integration: `kritva_failure_handling_integration` PASS (RUNNING -> injected failure -> ERROR event -> monitor-side observation -> controlled shutdown).
- Sanity: `kritva_failure_handling_sanity` PASS (sensor FAULT/UNHEALTHY, ERROR event, failed=1 affected=1, runtime STOPPED).
- Mutation check: disabling dependent propagation and disabling the FAULT reset each make the failure tests fail.
- Regression: `ctest --test-dir build/debug` 91/91 PASS (76 Core + 15 runtime). `make check` PASS. `git diff --check` clean.
- Core: unchanged (kritva-core-r1.0). No recovery is implemented (Core RECOVERING unused). A component that faults itself while RUNNING is not visible to Core's RuntimeManager (state stays RUNNING), so the host detects it from component observation (`failure_report()`).
