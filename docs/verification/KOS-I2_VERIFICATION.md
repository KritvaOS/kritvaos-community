# KOS-I2 Verification Report

**Milestone:** KOS-I2 Runtime / Application Foundation
**Dependency:** `kritva-core` R1.0 (`4acce3b`, unmodified)
**Result:** implemented (RR-CFG-003 enforced by `RuntimeHost` in a follow-up commit); independent audit CONDITIONAL PASS; findings addressed below; pending human review.

## Evidence

- Clean fresh clone (with submodule), Debug and Release, 0 warnings; `ctest` 94/94 PASS (76 Core + 18 runtime/demo).
- 94/94 PASS under AddressSanitizer + UndefinedBehaviorSanitizer (independent audit build).
- Builds and passes with `KRITVA_BUILD_EXAMPLES=OFF`.
- `make check` (source headers) PASS.
- Mutation checks (disable dependent propagation; disable FAULT reset) make the failure tests fail.

## Requirement coverage

| Requirements | Implementation | Tests |
|---|---|---|
| RR-LIF-001..008 | `runtime/src/runtime_host.cpp` over Core | `runtime_host_test`, `runtime_host_integration_test` |
| RR-CMP-001..008, RR-DEP-001..005 | `RuntimeHost::add_component` over Core registry/graph | `component_composition_*` |
| RR-CFG-001, 002, 004, 006 | `runtime/src/configuration.cpp`, `RuntimeHost::configure` | `configuration_*` |
| RR-CFG-003 | `RuntimeHost::initialize()` requires a prior successful `configure()` | `test_initialize_without_configuration_fails`, `test_missing_dependency`, `demo_application_test` |
| RR-OBS-001..007 | `RuntimeHost::observe`, `EventLog`, `describe` | `observation_*` |
| RR-FLT-001..007 | `failure_report`, `controlled_shutdown`, demo Sensor | `failure_handling_*`, `demo_components_test` |
| RR-APP-001..006 | `examples/kritva_demo/` | `demo_application_test`, `kritva_demo_reference_sanity` |
| RR-CFG-004; RR-SEC-002 | unknown-key rejection (`runtime.*` always; application allow-list), 64 KiB input limit, `runtime.name` charset | `test_unknown_keys_rejected`, `test_size_limit`, `test_runtime_name_validation`, configuration sanity |
| RR-FLT-007 | no recovery code; failed component stays FAULT until shutdown | `test_no_silent_recovery` (repeated rounds, no hook invoked, no state change) |
| RR-PERF-002, 003; RR-REL-003, 004 | bounded `EventLog`; deterministic demo | `test_event_log_bounded`, `test_deterministic_output`, repeat tests |

## Hardening (follow-up to the external audit of `709ee25`)

- Configuration: unknown `runtime.*` keys always rejected; applications may pass an allow-list (the demo does); text and files over 64 KiB rejected (files are never read in full); `runtime.name` limited to 1..64 characters of `[A-Za-z0-9_.-]`.
- `controlled_shutdown()` no longer emits a second ERROR event for a component that already failed; an independent stop failure is still reported.
- Stronger RR-FLT-007 test and an exact shutdown-hook assertion.

## Known limitations (audit INFO / MINOR, accepted for I2)

- Core `Event` has no payload or timestamp. Decision (ChatGPT, KOS-I2 review): Core R1.0 stays unchanged; an `Event` payload/timestamp is a future Core feature request, not part of KOS-I2.
- RR-OBS-006: no DEGRADED-health test.
- RR-REL-002: demo components hold no resources, so release is not meaningfully exercised.
- `docs/api/` does not exist yet; AGENTS.md section 23 submodule table still has its placeholder row.
