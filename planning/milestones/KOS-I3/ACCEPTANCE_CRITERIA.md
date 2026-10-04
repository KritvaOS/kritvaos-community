# KOS-I3 Milestone Acceptance Criteria

## Functional

- [x] All seven I3 tasks accepted.
- [x] Device and Endpoint contracts are documented.
- [x] Device and Endpoint identities are deterministic and unique.
- [x] Device discovery works by stable identifier.
- [x] Endpoint discovery works by Device + Endpoint identifier.
- [x] Endpoint capability is observable.
- [x] Sensor read and actuator write operations are typed.
- [x] Device/Endpoint lifecycle is coordinated by the runtime without a competing Core lifecycle engine.
- [x] Endpoint status and health are observable.
- [x] Endpoint statistics are observable.
- [x] Endpoint failure is deterministic and observable.
- [x] Mock hardware supports nominal and faulted operation.
- [x] Reference device demo completes the I3 flow.

## Verification

- [x] Unit tests PASS.
- [x] Integration tests PASS.
- [x] Sanity test PASS.
- [x] Complete KOS-I2 regression PASS.
- [x] Complete I3 regression PASS.
- [x] Clean build PASS.
- [x] `git diff --check` PASS.

## Architecture

- [x] Core R1.0 remains unchanged.
- [x] Device Registry is distinct from Core RuntimeManager registry.
- [x] No duplicated Core status/health/result/error contract.
- [x] No hardware transport dependency.
- [x] No physical driver dependency.
- [x] No ROS2/DDS dependency.
- [x] No Nexus/Edge transport implementation.
- [x] No speculative universal sensor/actuator data variant.
- [x] I3/I4 boundary remains explicit.

## Documentation

- [x] I3 milestone documents complete.
- [x] Device/Endpoint requirements added and traceable.
- [x] Runtime architecture updated for I3 boundary.
- [x] Repository architecture updated only where required.
- [x] Testing/regression strategy updated for I3.
- [x] Verification evidence recorded.
- [x] Changelog updated.

## Git

- [x] Each task has an atomic commit.
- [x] Working tree reviewed.
- [x] Commit diffs reviewed.
- [x] Commit hashes recorded.
- [x] No unrelated files committed.

## Evidence

- Task commits (each with its own recorded evidence in `I3-00x/ACCEPTANCE_CRITERIA.md`): I3-001 `39780cc`, I3-002 `a7e08e5`, I3-003 `97ab518`, I3-004 `f980675`, I3-005 `cd1a35c`, I3-006 `46b91fa`, I3-007 `d8cd315`.
- Independent audit remediation `40a6940`; re-audit follow-up `9912e3c`; planning closure `e61814e`. The historical task commits are not rewritten.
- Regression: 115/115 tests (76 Core + 18 KOS-I2 + 21 KOS-I3) in Debug and Release from a clean build, and in a fresh clone built in the project container; `make check` and `git diff --check` clean. Details in `docs/verification/KOS-I3_VERIFICATION.md`.
- Core: `git diff` of `core/` and of the KOS-I2 `runtime/` and `examples/kritva_demo/` is empty across the milestone.

## Open gate (not part of the checked items above)

- [ ] **Human safety review: OPEN.** `MotorCommand`, motor limits, fault-to-zero behavior, velocity command semantics, the motor mock's safety behavior and any future mapping to physical actuator control (AGENTS.md section 32). The software acceptance above does not close this gate; KOS-I3 is formally closed only when the owner signs it off.
