# Risks

| Risk | Mitigation |
|---|---|
| Runtime abstraction grows prematurely | Keep I2 implementation minimal; review new interfaces |
| Core boundary is violated | Treat Core R1.0 as frozen for normal I2 work |
| Tests cover only happy path | Require failure, sanity, and regression tests |
| Unrelated refactoring enters task | Enforce task-scoped acceptance and diff review |
