# KOS-I4 Verification Strategy

Record per-task SHA, Debug/Release results, unit/integration/sanity counts, I2/I3 regression counts, ASan/UBSan status and mutation/fault-injection evidence.

Required matrices: malformed frame; bounds; version; identity collisions; request/response correlation; drop/delay/reorder/disconnect; session transitions; timeout; stale/duplicate/replay; actuator limits; heartbeat safe stop; explicit recovery.

Run from a fresh clone/container before milestone closure. Any I2/I3 regression blocks I4 closure.

**HUMAN SAFETY REVIEW — OPEN (independent human-owned gate):** I4-005 to I4-008 and every actuator write path. It is not completed by software verification, blocks formal KOS-I4 closure and physical actuator deployment, and does not block subsequent software tasks. The I3 human safety review remains OPEN independently.

## Task evidence

| Task | Commit | Debug/Release | Regression (I2 + I3 + I4) | ASan/UBSan | Mutation |
|---|---|---|---|---|---|
| I4-001 | 5e9acfc, 6b73c6c, d2f8936 | PASS, 0 warnings | 119/119 | PASS | 20 of 20 caught |
| I4-002 | 52218f4 | PASS, 0 warnings | 123/123 | PASS | 26 of 28 caught by tests, 1 by ASan, 1 equivalent |
| I4-003 | subject `KOS-I4 I4-003 implement deterministic simulated transport` | PASS, 0 warnings | 127/127 | PASS | 33 of 33 caught (1 by hang timeout) |
| I4-004 | 3c535b6 | PASS, 0 warnings | 133/133 | PASS | 70 mutants: 56 caught first time, 8 survivors fixed with new tests, 6 equivalent |
| I4-005 | a690520 | PASS, 0 warnings | 137/137 | PASS | 63 mutants: 51 caught first time, 11 survivors fixed with new tests, 1 equivalent |
| I4-006 | subject `KOS-I4 I4-006 implement distributed failure handling and actuator safety` | PASS, 0 warnings | 139/139 | PASS | 61 mutants: 56 caught first time, 2 survivors fixed with new tests, 3 equivalent |

Per-task detail is in `planning/milestones/KOS-I4/I4-00x/ACCEPTANCE_CRITERIA.md`.
