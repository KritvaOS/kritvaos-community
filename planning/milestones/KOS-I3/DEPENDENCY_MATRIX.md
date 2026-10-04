# KOS-I3 Dependency Matrix

| Task | Depends on | Produces | Consumers |
|---|---|---|---|
| I3-001 | KOS-I2 | Device/Endpoint contracts | I3-002..007 |
| I3-002 | I3-001 | Registry/discovery | I3-003, I3-005, I3-007 |
| I3-003 | I3-001, I3-002 | Runtime lifecycle integration | I3-006, I3-007 |
| I3-004 | I3-001 | Typed data contracts | I3-005, I3-007 |
| I3-005 | I3-002, I3-004 | Mock devices/endpoints | I3-006, I3-007 |
| I3-006 | I3-003, I3-005 | Diagnostics/fault behavior | I3-007 |
| I3-007 | I3-001..006 | End-to-end I3 proof | KOS-I4 entry baseline |

## Architectural Dependencies

```text
kritva-core R1.0
       ↓
KOS-I2 Runtime
       ↓
I3-001 Contracts
       ↓
I3-002 Registry
       ↓
I3-003 Lifecycle ─────┐
       ↓              │
I3-004 Typed Data ────┤
       ↓              │
I3-005 Mocks ─────────┤
       ↓              │
I3-006 Diagnostics ───┘
       ↓
I3-007 Demo
```
