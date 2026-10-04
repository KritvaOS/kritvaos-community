# KOS-I3 Test Plan

## Test Levels

| Level | Purpose |
|---|---|
| Unit | Device, Endpoint, registry, data contract and mock behavior |
| Component | Device lifecycle and endpoint behavior |
| Integration | Runtime + registry + devices + endpoints |
| System | Reference device demo |
| Sanity | Clean build, launch, expected flow, clean shutdown |
| Regression | KOS-I2 plus previously accepted repository behavior |

## Mandatory Sequence

```text
Build → Unit → Integration → Sanity → Regression → Review → Commit
```

## Core Test Areas

1. Identity and duplicate rejection.
2. Device registration/discovery.
3. Endpoint registration/discovery.
4. Capability reporting.
5. Lifecycle ordering.
6. Sensor read.
7. Actuator write.
8. Invalid operation handling.
9. Statistics.
10. Fault injection.
11. Health/status consistency.
12. Controlled shutdown.
13. Deterministic repeated execution.

## Required Negative Tests

- duplicate Device ID;
- duplicate Endpoint ID;
- unknown Device ID;
- unknown Endpoint ID;
- operation before initialization;
- operation after stop;
- sensor read failure;
- actuator write failure;
- faulted endpoint operation;
- invalid lifecycle transition;
- shutdown after endpoint failure.

## Regression Gate

All existing KOS-I2 tests must remain passing. Any regression blocks task acceptance unless explicitly reviewed and documented.
