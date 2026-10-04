# I3-004 Acceptance Criteria

## Functional
- [x] Typed sensor read contract defined.
- [x] Typed actuator write contract defined.
- [x] `AccelerationSample` and `AngularVelocitySample` contracts defined (replaces the single `ImuSample`; DECISIONS #4).
- [x] `PositionSample` contract defined (replaces `EncoderSample`; DECISIONS #4).
- [x] `MotorCommand` contract defined.
- [x] Units and validity semantics documented.
- [x] Operation-before-ready behavior defined.
- [x] Operation-after-stop behavior defined.
- [x] Failure result semantics defined.
- [x] No universal sensor/actuator data variant introduced.

## Unit
- [x] Sample construction/default tests PASS.
- [x] Validity/units tests PASS.
- [x] Read/write contract tests PASS.
- [x] Error-path tests PASS.

## Integration
- [x] Typed endpoints compile and operate through the Device contract.

## Sanity
- [x] Clean build PASS.
- [x] Deterministic sample/command scenario PASS.

## Regression
- [x] Complete KOS-I2 suite PASS.

## Review
- [x] Data contracts are minimal.
- [x] No real-time guarantee introduced.
- [x] No transport-specific data structure introduced.

## Git
- [x] Diff reviewed.
- [x] Atomic commit.
- [x] Commit hash recorded: `f980675`.

## Evidence (I3-004)

- Build: Debug, 0 warnings from KOS-I3 code.
- Unit: `kritva_hardware_typed_unit` PASS (default state, validity incl. NaN/Inf per axis, MotorCommand validation matrix with inclusive bounds, limit validation, deterministic virtual timestamps, capability descriptions, conformance of all four typed endpoints, operation before ready / after stop, deterministic sequences, rejected commands have no effect).
- Integration: `kritva_hardware_typed_integration` PASS (typed endpoints found by name through the registry, wrong type detectable, operated under the DeviceManager, NOT_READY before start and after stop, rejected command not applied, statistics).
- Sanity: `kritva_hardware_header_sanity` PASS (now includes `samples.hpp` and `typed_endpoints.hpp`).
- Regression: `ctest` 106/106 PASS. `make check` PASS.
- Mutation checks (limit bound, NaN check, timestamp, validity) each fail the tests.
- Core: unchanged.
- Decision applied: per-endpoint samples (`AccelerationSample`, `AngularVelocitySample`, `PositionSample`, `MotorCommand`) replace the single `ImuSample`/`EncoderSample` (DECISIONS #4); no universal variant.
- **HUMAN REVIEW REQUIRED (AGENTS.md section 32, physical actuator control):** the `MotorCommand` contract (single velocity target in rad/s, inclusive limits, default [-1, 1], validate-before-act, rejected command has no effect, NaN/Inf rejected). Not yet reviewed by a human.
