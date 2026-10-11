# status/

`status.json` reports this repository's task-level facts to the Kritva program (schema 2.0, defined in `KritvaOS/kritva-program` under `schema/status.schema.json`).

Generate it with `python3 scripts/gen_status.py` (reads `planning/milestones/*/TASKS.md`, per-task `ACCEPTANCE_CRITERIA.md` paths, and git state). Never hand-edit it.

## What the fields mean (and do not mean)

- **execution**: `COMPLETE` means the task's `TASKS.md` status says Done (implementation done). It is not acceptance, not review approval, and says nothing about safety gates.
- **git_sync**: `SYNCED` only when git confirms a clean working tree with HEAD equal to `origin/main`. Without git the generator cannot verify cleanliness and reports `COMMITTED`.
- **readiness**: always `IMPLEMENTABLE` in this report. It means only that software work can proceed subject to central gates. It is a fixed default, not a per-task finding, and not safety clearance. The open human safety reviews (KOS-I3, KOS-I4) and deferred KOS-CI-001 are tracked centrally in kritva-program (`v2/tracking/baseline.json`, `open_gates`) and block gated activity.
- **evidence**: the `ACCEPTANCE_CRITERIA.md` path is a criteria reference, not proof of completion. Entries have no `commit`, `ci_run` or `report_sha256`, so they can never support a PASS.
- **source.commit**: the inspected source revision (HEAD when generated). The file is committed afterwards, so it describes its parent state; its own commit is not claimed. Regenerate after each task or milestone change. A CI-generated artifact bound to the tested SHA and run ID is a planned improvement.

## Authority

This repo reports facts only. Review and acceptance are decided centrally in kritva-program `v2/tracking/approvals.json` (approver: Dinesh). All 21 tasks earn zero progress credit until evidence-backed approvals are recorded there.
