# I3-001 Acceptance Criteria

## Functional
- [x] Device identity contract defined.
- [x] Endpoint identity contract defined.
- [x] Device-to-Endpoint ownership relationship defined.
- [x] Capability contract defined.
- [x] Lifecycle contract defined.
- [x] Status/health/error semantics defined.
- [x] Sensor and actuator specialization boundary defined.
- [x] Scope limited to I3-001.

## Unit
- [x] Contract tests cover valid identities.
- [x] Contract tests cover invalid identities.
- [x] Capability behavior tested.
- [x] Unit tests PASS.

## Integration
- [x] Applicable runtime integration contract test PASS.

## Sanity
- [x] Clean build PASS.
- [x] Public headers compile in the supported build.
- [x] No physical driver or transport dependency.

## Regression
- [x] KOS-I2 tests PASS.
- [x] No unexpected regression.

## Review
- [x] Core R1.0 boundary respected.
- [x] No duplicate Core types.
- [x] No competing lifecycle engine.
- [x] No universal sensor/actuator variant introduced.
- [x] I3/I4 boundary documented.

## Git
- [x] Diff reviewed.
- [x] Atomic commit.
- [ ] Commit hash recorded after commit.

## Evidence (I3-001)

- Build: clean `rm -rf build`, Debug and Release presets: 0 warnings from KOS-I3/KOS-I2 code (Core's own `component_context_test` has one GCC 13 `-Wpessimizing-move` warning; Core is unchanged).
- Unit: `kritva_hardware_identity_unit`, `kritva_hardware_endpoint_unit` (conformance suite on three test doubles, failing hooks, shutdown periods, degraded health, typed read/write, capabilities), `kritva_hardware_device_unit` (ownership, ordering, rejections, fixed endpoint set, aggregation) PASS.
- Integration: `kritva_hardware_runtime_integration` PASS (a Core Component drives a Device's endpoints through the unchanged RuntimeHost; endpoint fault visible in `failure_report()`; controlled shutdown).
- Sanity: `kritva_hardware_header_sanity` PASS (each public header compiles alone); configure-time check that `kritva_hardware` links only `kritva_core`.
- Regression: `ctest` 99/99 PASS in Debug and Release (76 Core + 18 I2 + 5 I3-001). `make check` PASS. `git diff --check` clean.
- Mutation checks (stop from FAULT, wrong fault error code, initialize failure not faulting, device health aggregation, late endpoint add) each fail the tests.
- Core: unchanged (kritva-core-r1.0). No new Core types: status, health, result, error, capability, statistics and lifecycle are Core's.
- Review notes: the typed samples (I3-004) and mocks (I3-005) are not part of this task. The actuator contract documents validate-before-act; the safety review of `MotorCommand` belongs to I3-004/I3-005.
