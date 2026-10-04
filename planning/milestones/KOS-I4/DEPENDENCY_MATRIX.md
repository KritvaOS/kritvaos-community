# KOS-I4 Dependency Matrix

| Task | Depends on | Output |
|---|---|---|
| I4-001 | I3 | contract |
| I4-002 | I4-001 | frame/codec |
| I4-003 | I4-002 | transport |
| I4-004 | I4-003 + I3 | Edge service |
| I4-005 | I4-004 + I3 | remote proxies |
| I4-006 | I4-005 | failure/safety |
| I4-007 | I4-006 + I2 | runtime/diagnostics |
| I4-008 | I4-007 | demo/evidence |
