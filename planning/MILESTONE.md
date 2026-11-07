# Active Milestone

Current milestone: **KOS-I4 — Nexus↔Edge Integration** (software implementation complete; human safety review OPEN).

See `milestones/KOS-I4/` and `MILESTONE_STATUS.md`. The predecessor KOS-I3 (`milestones/KOS-I3/MILESTONE.md`) keeps its own, independent OPEN human safety review.

## V2 Program Milestone Alignment

This section links Community's planned work to the V2 program milestones owned by `KritvaOS/kritva-program`. It is a reading aid. It adds no task, changes no status, closes no milestone and creates no program credit.

Authority (kritva-program D-011, D-093 to D-096):

- The program owns the V2 milestone plan: [v2/planning/milestones.json](https://github.com/KritvaOS/kritva-program/blob/main/v2/planning/milestones.json), rendered in [V2_MILESTONE_PLAN.md](https://github.com/KritvaOS/kritva-program/blob/main/v2/planning/V2_MILESTONE_PLAN.md). Task-to-milestone mapping lives in the program baseline. Community does not assert a program milestone in `status/status.json`.
- Each Community repository milestone below uses the ID the program already records, `V2-M<n>-COMMUNITY-<seq>`, and maps to exactly one program milestone. Cross-milestone dependencies are recorded at program level only.
- A milestone closes only through an approved `COMPLETION_RECORD.md` in the program's acceptance standard. A merge, a CI run or a tag does not close one.
- Task counts, weight and closure state are in the program's generated [STATUS_DETAIL.md](https://github.com/KritvaOS/kritva-program/blob/main/v2/tracking/STATUS_DETAIL.md). They are not repeated here.

### Community repository milestones

| Repository milestone | Program milestone | Outcome (as recorded by the program) | Status | Tasks |
|---|---|---|---|---|
| `V2-M0-COMMUNITY-001` | V2-M0 | Security reporting path working (SECURITY.md contact tested). | IN PROGRESS | `KOS-COMMUNITY-M0-001` (baselined in program v11, D-117; see `milestones/KOS-M0/TASKS.md`) |
| `V2-M1-COMMUNITY-001` | V2-M1 | Public architecture description matches the approved capability contracts. | PROPOSED | None yet |
| `V2-M3-COMMUNITY-001` | V2-M3 | Public integration guide matches the control-plane contract. | PROPOSED | None yet |
| `V2-M7-COMMUNITY-001` | V2-M7 | Public documentation reflects the live and results interfaces. | PROPOSED | None yet |
| `V2-M8-COMMUNITY-001` | V2-M8 | Release, security disposition and licence review complete. | PROPOSED | None yet |

Community has no planned repository milestone in V2-M2, V2-M4, V2-M5 or V2-M6. Do not add one here to fill the gap; that is a program decision.

`V2-M0-COMMUNITY-001` is tied to the pending program decision PD-017 (test the Community security mailbox after the SECURITY.md contact patch merges). No test email is sent under this milestone before the owner decides.

### Historical milestones

`KOS-I2`, `KOS-I3` and `KOS-I4` are classified `PRE_V2` by the program (D-094). They sit outside V2-M0 to V2-M8, no V2 milestone closes by reference to them, and no weight or credit transfers from them. Their own records under `planning/milestones/` and their human safety review state are unchanged by this section.

### Rules for this section

- Use the existing IDs. Do not introduce a second scheme.
- Do not add tasks under a PROPOSED milestone until the program allocates real task IDs and baselines them.
- Update this section only after the program records a change in `milestones.json`; the program record wins on any disagreement.
