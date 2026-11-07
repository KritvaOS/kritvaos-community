# KOS-COMMUNITY-M0-001 — Acceptance Criteria

Status: IN PROGRESS (criteria drafted from kritva-program `v2/planning/M0_TASK_DEFINITIONS.md`; owner review and central approval pending)

## Acceptance Criteria

1. `SECURITY.md` names a reporting contact.
2. A test message to that contact is received and recorded in `planning/PD-017-SECURITY-MAILBOX-TEST.md` (date, time and who received it), and GitHub private vulnerability reporting is confirmed enabled.
3. The record's outcome is PASS and it is merged by pull request.
4. The program readiness check `security_policy` passes (moves from OPEN to ATTESTED).
5. Evidence is a merged pull request in this repository and a passing CI run on the merged commit, recorded in `status/evidence.json` once the task is complete.
6. No other repository's records are changed by this task.

## Authority

This file states criteria only. It does not accept the task and does not edit the protected `SECURITY.md`. Acceptance, approval and credit are decided in kritva-program. No test message is sent until the owner runs the PD-017 test.
