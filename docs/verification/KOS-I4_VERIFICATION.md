# KOS-I4 Verification Strategy

Record per-task SHA, Debug/Release results, unit/integration/sanity counts, I2/I3 regression counts, ASan/UBSan status and mutation/fault-injection evidence.

Required matrices: malformed frame; bounds; version; identity collisions; request/response correlation; drop/delay/reorder/disconnect; session transitions; timeout; stale/duplicate/replay; actuator limits; heartbeat safe stop; explicit recovery.

Run from a fresh clone/container before milestone closure. Any I2/I3 regression blocks I4 closure.

**HUMAN SAFETY REVIEW REQUIRED:** I4-006 and every actuator write path. I3 human safety review remains OPEN independently.
