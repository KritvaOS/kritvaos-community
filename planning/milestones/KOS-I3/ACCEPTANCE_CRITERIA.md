# KOS-I3 Milestone Acceptance Criteria

## Functional

- [ ] All seven I3 tasks accepted.
- [ ] Device and Endpoint contracts are documented.
- [ ] Device and Endpoint identities are deterministic and unique.
- [ ] Device discovery works by stable identifier.
- [ ] Endpoint discovery works by Device + Endpoint identifier.
- [ ] Endpoint capability is observable.
- [ ] Sensor read and actuator write operations are typed.
- [ ] Device/Endpoint lifecycle is coordinated by the runtime without a competing Core lifecycle engine.
- [ ] Endpoint status and health are observable.
- [ ] Endpoint statistics are observable.
- [ ] Endpoint failure is deterministic and observable.
- [ ] Mock hardware supports nominal and faulted operation.
- [ ] Reference device demo completes the I3 flow.

## Verification

- [ ] Unit tests PASS.
- [ ] Integration tests PASS.
- [ ] Sanity test PASS.
- [ ] Complete KOS-I2 regression PASS.
- [ ] Complete I3 regression PASS.
- [ ] Clean build PASS.
- [ ] `git diff --check` PASS.

## Architecture

- [ ] Core R1.0 remains unchanged.
- [ ] Device Registry is distinct from Core RuntimeManager registry.
- [ ] No duplicated Core status/health/result/error contract.
- [ ] No hardware transport dependency.
- [ ] No physical driver dependency.
- [ ] No ROS2/DDS dependency.
- [ ] No Nexus/Edge transport implementation.
- [ ] No speculative universal sensor/actuator data variant.
- [ ] I3/I4 boundary remains explicit.

## Documentation

- [ ] I3 milestone documents complete.
- [ ] Device/Endpoint requirements added and traceable.
- [ ] Runtime architecture updated for I3 boundary.
- [ ] Repository architecture updated only where required.
- [ ] Testing/regression strategy updated for I3.
- [ ] Verification evidence recorded.
- [ ] Changelog updated.

## Git

- [ ] Each task has an atomic commit.
- [ ] Working tree reviewed.
- [ ] Commit diffs reviewed.
- [ ] Commit hashes recorded.
- [ ] No unrelated files committed.
