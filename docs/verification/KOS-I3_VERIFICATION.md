# KOS-I3 Verification Report

**Milestone:** KOS-I3 Device & Endpoint Integration
**Status:** IMPLEMENTED; independent audit findings remediated; **HUMAN SAFETY REVIEW REQUIRED — OPEN**
**Dependency:** `kritva-core` R1.0 (`4acce3b`, unmodified)

## Evidence

- Clean fresh clone with the `kritva-core` submodule, built and tested inside the project container `kritvaos-dev:0.1` (the CI commands): `ctest` 115/115 PASS.
- Clean local build, Debug and Release: `ctest` 115/115 PASS (76 Core + 18 KOS-I2 + 21 KOS-I3), 0 warnings from KOS-I2/KOS-I3 code. GCC 13 reports one `-Wpessimizing-move` warning in Core's own `component_context_test.cpp`; Core is out of scope and unchanged.
- The I3 unit and integration tests also ran clean under AddressSanitizer + UndefinedBehaviorSanitizer; this found and fixed a dangling temporary in `DeviceManager::capabilities()` during I3-003.
- `make check` (source headers) PASS; `git diff --check` clean.
- Mutation checks were run for each task (see each `I3-00x/ACCEPTANCE_CRITERIA.md`); every mutation of production code was caught.
- Configure-time checks: `kritva_hardware` links only `kritva_core`, `kritva_hardware_mock` only `kritva::hardware`.

## Task commits

| Task | Commit |
|---|---|
| I3-001 | 39780cc |
| I3-002 | a7e08e5 |
| I3-003 | 97ab518 |
| I3-004 | f980675 |
| I3-005 | cd1a35c |
| I3-006 | 46b91fa |
| I3-007 | d8cd315 |

## Audit remediation

An independent read-only audit of `d8cd315` found one blocker, three major and several minor items; all were accepted (ChatGPT review) and fixed in one follow-up commit, with a test for each. The seven task commits are not rewritten.

| Finding | Fix | Test |
|---|---|---|
| B1 architecture document truncated in 97ab518 | restored from a7e08e5, I3-003..I3-006 re-added, guarded writer for doc edits | content check of every section |
| M1 `stop()` aborted at the first failing endpoint | best-effort stop of all endpoints, first error returned | `test_failing_stop_still_stops_every_endpoint` (all four positions of the failing endpoint) |
| M2 `shutdown()` swallowed stop failures | first stop/shutdown error returned, state unchanged, retry resumes | `test_shutdown_returns_the_first_stop_error`, `test_failing_shutdown_hook_is_reported_and_retried` |
| M3 failed `configure()` committed the device switches | validate first, commit switches only on success; endpoint settings applied in order without rollback (as Core) | `test_failed_configure_does_not_commit_device_switches` |
| m1 motor limits truncated, unbounded, mutable while live | lossless, +/-1000 ceiling, frozen once operational | `test_fractional_limits_survive_configuration`, `test_set_limits_ceiling_and_lifecycle` |
| m2 faulted motor kept a stale model velocity | additive `Endpoint::on_fault()` hook, motor zeroes the model | `test_faulted_motor_model_is_zero` (reads `model()` directly) |
| m3 sample validity not enforced | `read()` checks `is_valid()` on a temporary; caller's sample untouched on failure | `test_invalid_samples_are_never_reported_as_success` |
| m4 `describe()` forgeable by quotes and capability names | quotes, control characters and list commas replaced | `test_quotes_cannot_forge_fields`, `test_capability_names_are_sanitised` |
| m5 hook-induced FAULT could double-notify (Release) | operation fails without a second transition; docs corrected | `test_hook_that_faults_the_endpoint_fails_the_operation` |
| m6 fault listener slot cleared unconditionally | owner-scoped listeners | `test_fault_listeners_are_owner_scoped`, `test_a_destroyed_manager_does_not_remove_anothers_listener` |
| m7 endpoint set not fully closed | `Device::seal()` at `DeviceManager::initialize()` | `test_devices_are_sealed_at_initialize` |
| m8 overstated tests and claims | `static_assert`s that Device/Endpoint are not Core Components; tautological test removed; I3-007 acceptance corrected | `test_device_and_endpoint_are_not_core_components` |

Every fix was also checked by reverting it (mutation): each reversal fails at least one test.

## Requirement coverage

| Requirements | Implementation | Tests |
|---|---|---|
| DER-001, 002, 004, 005, 006 | `hardware/abstraction` depends only on Core; mocks use the same contracts | link checks; `endpoint_test` conformance on test doubles and `mock_test` conformance on mocks |
| DER-003 | Devices/Endpoints are not Core Components; one `DeviceManager` Component | `device_manager_test`, `device_manager_runtime_integration_test` |
| DER-101..108 | `DeviceRegistry`, `Device::add_endpoint` | `device_registry_test`, `device_test`, `registry_discovery_integration_test` |
| DER-201..206 | `Device`, `DeviceInfo`, aggregation, ownership | `device_test`, `identity_test`, `device_manager_test` |
| DER-301..307 | `Endpoint`, `EndpointInfo` | `endpoint_test` + conformance suite |
| DER-401..407 | `Endpoint` lifecycle on Core `Lifecycle`; `DeviceManager` ordering | conformance (exhaustive table), `device_manager_test` (exact reverse order), `device_manager_runtime_integration_test` |
| DER-501..507 | `samples.hpp`, `SensorEndpoint<T>`, `ActuatorEndpoint<T>` | `typed_data_test`, `typed_discovery_integration_test` |
| DER-601..608 | `DeviceManager::diagnostics()`, `describe()`, I2 observation | `diagnostics_test`, `fault_observation_integration_test`, `kritva_hardware_diagnostics_sanity` |
| DER-701..706 | fault handling, mocks, no recovery | `endpoint_test`, `mock_test`, `device_manager_test` (no silent recovery), `fault_observation_integration_test` |
| DER-801..805 | test doubles, `kritva_hardware_mock`, demo | `mock_test`, `device_demo_test`, `kritva_device_demo_sanity`; clean-clone and container builds |
| DER-901..905 | no transport, fieldbus or Nexus/Edge dependency | link checks; the contracts carry no transport concept |

## Decisions applied (planning/DECISIONS.md)

Single `DeviceManager` Component (no `add_device`), Core `Lifecycle` reuse, one-ERROR fault policy, per-endpoint typed samples, strong ids with names, non-owning registry, `hardware/mock/` library, Core error codes only, virtual timestamps, motor safety contract, `<device>.<endpoint>.<setting>` keys, strictly sequential atomic commits.

## Known limitations and review items

- **HUMAN REVIEW REQUIRED — OPEN (AGENTS.md section 32, physical actuator control):** the `MotorCommand` contract (I3-004), motor limits, fault-to-zero behavior, velocity command semantics, the motor mock's safety behavior (I3-005) and any future mapping of this contract to physical actuator control. Not reviewed by a human; the software audit remediation does not close this gate.
- Mock settings are integers (rad/s limits via configuration; fractional limits through `set_limits()`), because the I2 configuration loader parses only bool, integer and string values.
- The demo's own defensive checks (no recovery, error-event count) fire only if the library misbehaves, so removing them is not detected by the demo tests; the underlying behavior is covered by the library tests.
- Fault listeners are owner-scoped; a registrant must clear its own before it is destroyed. A faulting endpoint is reported to the manager as a single ERROR event without endpoint identity (Core `Event` has no payload); the identity is in `diagnostics()`.
- `DeviceManager` requires registered Devices to outlive it (documented; the destructor clears the listeners it installed).
- No sanitizer, clang-tidy or Clang matrix stage in CI yet (deferred).
- `docs/api/` does not exist yet; AGENTS.md section 23 submodule table still has its placeholder row.

## Acceptance rule

KOS-I3 is complete when all task and milestone acceptance criteria are satisfied and recorded, the independent audit findings are addressed, and the safety-flagged items have been reviewed by a human.
