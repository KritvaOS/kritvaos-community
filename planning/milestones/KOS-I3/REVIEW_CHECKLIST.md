# KOS-I3 Review Checklist

## Architecture

- [ ] Device/Endpoint belongs below Runtime and above drivers/hardware.
- [ ] I3 remains transport-neutral.
- [ ] I4 transport concerns are not introduced.
- [ ] Device Registry is not a duplicate Core component registry.
- [ ] No second global lifecycle state machine.
- [ ] Core R1.0 boundary respected.
- [ ] No speculative abstractions.

## Contracts

- [ ] Device identity is stable.
- [ ] Endpoint identity is stable within a Device.
- [ ] Capability reporting is explicit.
- [ ] Typed sensor/actuator contracts are understandable.
- [ ] Error semantics are deterministic.
- [ ] Status/health use released Core contracts.

## Implementation

- [ ] Ownership is clear.
- [ ] Lifetime is clear.
- [ ] Invalid operations are handled.
- [ ] Fault behavior is deterministic.
- [ ] Mock behavior is deterministic.
- [ ] No hardware dependencies in abstraction tests.

## Verification

- [ ] Unit PASS.
- [ ] Integration PASS.
- [ ] Sanity PASS.
- [ ] KOS-I2 regression PASS.
- [ ] I3 regression PASS.
- [ ] `git diff --check` PASS.

## Repository

- [ ] No unrelated changes.
- [ ] Documentation updated.
- [ ] No generated/temp files.
- [ ] Correct directory ownership.

## Git

- [ ] Atomic commit.
- [ ] Diff reviewed.
- [ ] Commit message compliant.
- [ ] Commit hash recorded.
