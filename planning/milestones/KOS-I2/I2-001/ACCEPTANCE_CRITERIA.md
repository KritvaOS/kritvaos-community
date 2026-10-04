# I2-001 Acceptance Criteria

## Functional
- [x] Functional requirements for I2-001 implemented.
- [x] Error paths are deterministic.
- [x] Scope remains limited to I2-001.

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
- [ ] Commit hash recorded (in planning/MILESTONE_STATUS.md after commit).

## Evidence (I2-001)

- Configure/build: `cmake --preset debug` + `cmake --build build/debug` from a removed `build/`: PASS, 0 warnings (Debug and Release).
- Unit: `kritva_runtime_host_unit` PASS (7 test functions; INITIALIZING/STOPPING observed via probe component).
- Integration: `kritva_runtime_host_integration` PASS (dependency-ordered start, reverse stop).
- Sanity: `kritva_runtime_host_sanity` PASS; `kritva_demo` prints UNKNOWN, READY, RUNNING, STOPPED and exits 0.
- Regression: `ctest --test-dir build/debug`: 79/79 PASS (76 Core + 3 I2-001). `make check` PASS. `git diff --check` clean.
- Note: Core's INITIALIZING and STOPPING are transient (not observable once an operation returns), so the demo output shows READY/RUNNING/STOPPED; the transient states are verified by the unit and integration tests.
- Core: `core/` unchanged, pinned at kritva-core-r1.0 (4acce3b).
