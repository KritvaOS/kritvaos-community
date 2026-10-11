# status/

`status.json` reports this repository's task-level facts to the Kritva program (schema 2.0, defined in `KritvaOS/kritva-program` under `schema/status.schema.json`).

- Generated, not hand-edited: `python3 scripts/gen_status.py` (reads `planning/milestones/*/TASKS.md`, per-task `ACCEPTANCE_CRITERIA.md` and git state).
- Reports facts only (execution, git sync, readiness, evidence). Review and acceptance are decided centrally in kritva-program `v2/tracking/approvals.json`; this repo never self-accepts.
- Evidence entries have no `commit`, `ci_run` or `report_sha256` yet, so the program grants no progress credit until commit-bound CI evidence is added.
- `source.commit` is the commit the file was generated against; regenerate after each task or milestone change.
- Check for drift: `python3 scripts/gen_status.py --check --updated-at <value from the file>`.
