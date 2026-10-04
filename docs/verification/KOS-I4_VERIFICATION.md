# KOS-I4 Verification Strategy

Record per-task SHA, Debug/Release results, unit/integration/sanity counts, I2/I3 regression counts, ASan/UBSan status and mutation/fault-injection evidence.

Required matrices: malformed frame; bounds; version; identity collisions; request/response correlation; drop/delay/reorder/disconnect; session transitions; timeout; stale/duplicate/replay; actuator limits; heartbeat safe stop; explicit recovery.

Run from a fresh clone/container before milestone closure. Any I2/I3 regression blocks I4 closure.

**HUMAN SAFETY REVIEW — OPEN (independent human-owned gate):** I4-005 to I4-008 and every actuator write path. It is not completed by software verification, blocks formal KOS-I4 closure and physical actuator deployment, and does not block subsequent software tasks. The I3 human safety review remains OPEN independently.

## Task evidence

| Task | Commit | Debug/Release | Regression (I2 + I3 + I4) | ASan/UBSan | Mutation |
|---|---|---|---|---|---|
| I4-001 | recorded in the next commit | PASS, 0 warnings | 119/119 | PASS | 15 of 15 caught |

Per-task detail is in `planning/milestones/KOS-I4/I4-00x/ACCEPTANCE_CRITERIA.md`.
