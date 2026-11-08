# PD-017: security mailbox test record (V2-M0, kritvaos-community)

Status: WAIVED by the owner (program decision D-119, 2026-11-08). The test was not performed and no result is recorded below.
Program record: PD-017 in kritva-program `v2/planning/PENDING_DECISIONS.md`. Readiness check: `security_policy` (OPEN until this record is complete).
Related: SECURITY.md names the security contact and GitHub private vulnerability reporting; D-068 records the Security Owner's statement that the route was tested on 18 October 2026. That statement is not evidence; this record replaces it with a dated test.

This file records evidence only. It does not edit the protected `SECURITY.md`, accept a task, or award progress.

## Test steps (Security Owner)

1. Send a message from an address outside the contact's own mailbox to the security contact listed in `SECURITY.md`. Subject: `PD-017 security route test`. Put no vulnerability detail in it.
2. Confirm it arrived in the contact's inbox (not spam) and note the received date and time.
3. Open the repository Security tab and confirm "Report a vulnerability" is enabled (private reporting). Do not submit a report.
4. Fill in the table below and merge this file by pull request.

## Result

| Item | Value |
|---|---|
| Date and time sent (UTC) | ____ |
| Sent from | ____ |
| Contact address used (as in SECURITY.md) | ____ |
| Received by (person) | ____ |
| Date and time received (UTC) | ____ |
| Landed in inbox, not spam | yes / no |
| GitHub private vulnerability reporting enabled | yes / no |
| Repository commit when tested | ____ |

## Outcome

Outcome: WAIVED (not PASS: the mailbox test and the private reporting check were not recorded)
Recorded by: Dinesh (owner decision D-119)
Date: 2026-11-08

The program closed PD-017 as waived (D-119) and removed KOS-COMMUNITY-M0-001 from the baseline. `security_policy` is not moved to ATTESTED by this record. If the owner later runs the test, fill in the table and record the result by pull request.
