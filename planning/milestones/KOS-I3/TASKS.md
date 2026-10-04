# KOS-I3 Tasks

| Task | Description | Primary output | Dependency | Unit | Integration | Sanity | Regression | Status |
|---|---|---|---|---|---|---|---|---|
| I3-001 | Device / Endpoint architecture and contracts | Stable hardware abstraction interfaces | KOS-I2 | Required | Required | Required | Required | Planned |
| I3-002 | Device and Endpoint registry | Deterministic discovery/registration | I3-001 | Required | Required | Required | Required | Planned |
| I3-003 | Endpoint lifecycle and Runtime integration | Runtime-owned device lifecycle coordination | I3-001, I3-002 | Required | Required | Required | Required | Planned |
| I3-004 | Typed sensor / actuator data interfaces | Minimal typed read/write contracts | I3-001 | Required | Required | Required | Required | Planned |
| I3-005 | Mock Device / Endpoint implementation | Deterministic mock hardware | I3-002, I3-004 | Required | Required | Required | Required | Planned |
| I3-006 | Observation, diagnostics and fault handling | Status/health/statistics/failure behavior | I3-003, I3-005 | Required | Required | Required | Required | Planned |
| I3-007 | Reference Device/Endpoint demo | End-to-end I3 proof | I3-001..006 | Required | Required | Required | Required | Planned |

## Execution Order

```text
I3-001
   ↓
I3-002
   ↓
I3-003 ─────┐
   ↓        │
I3-004 ─────┤
   ↓        │
I3-005 ─────┤
   ↓        │
I3-006 ─────┘
   ↓
I3-007
```

I3-003 and I3-004 may be developed in parallel after I3-001, but acceptance remains dependency-ordered.
