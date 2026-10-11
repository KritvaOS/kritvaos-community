#!/usr/bin/env python3
#==============================================================================
# Copyright (c) 2026 KritvaOS
# SPDX-License-Identifier: Apache-2.0
#
# File        : gen_status.py
# Description : Generate status/status.json (kritva-program schema 2.0) from planning records
#
# Component   : Infrastructure
# Module      : Program Status Reporting
# Layer       : Development Infrastructure
#
# Requirements: kritva-program decisions D-009, D-011, D-013
# API         : Command-line status report generator
#
# Author      : KritvaOS
# Created     : 11-10-2026
#==============================================================================
"""Generate status/status.json (kritva-program schema 2.0) for kritvaos-community.

Reports FACTS only, derived from this repo's own planning records:
  planning/milestones/<KOS-Ix>/TASKS.md   task rows and their Status text
  planning/milestones/<KOS-Ix>/<task>/ACCEPTANCE_CRITERIA.md  evidence pointer
  git (or .git/refs when git is unavailable)  source commit and sync state
It never reports review or acceptance: those are decided centrally in
kritva-program (v2/tracking/approvals.json). Evidence entries carry no
`commit`, `ci_run` or `report_sha256` because the planning records only hold
short SHAs and no CI run IDs; the central consolidator therefore grants no
progress credit until real, commit-bound evidence is added.

Field semantics (kept deliberately conservative):
  * execution: TASKS.md "Done" means implementation done. It does NOT mean
    acceptance passed or review approved, and says nothing about safety gates.
  * git_sync: SYNCED only when git confirms a clean tree whose HEAD equals
    origin/main. Without git, cleanliness is unverified, so COMMITTED is used.
  * readiness: always IMPLEMENTABLE here. It means only "software work can
    proceed, subject to central gates"; it is a fixed default, not a
    task-specific finding, and not safety clearance. Open human safety reviews
    and deferred CI gates stay central in kritva-program (baseline.json
    open_gates) and block gated activity.
  * evidence: the ACCEPTANCE_CRITERIA.md path is a CRITERIA REFERENCE, not proof
    of completion. It has no commit or CI binding and can never support a PASS.
  * source.commit: the inspected source revision (HEAD at run time). The
    generated file itself is committed later, so it describes its parent state;
    its own commit identity is not claimed.

Usage: python3 scripts/gen_status.py [--root .] [--out status/status.json]
       [--updated-at 2026-10-11T00:00:00Z] [--check | --check-facts]
  --check        local: full comparison (also source commit and git_sync)
  --check-facts  CI: compare only the planning-derived facts (task IDs,
                 execution, evidence pointers). source.commit, git_sync and
                 updated_at legitimately differ from CI's checkout, so they
                 are ignored.
Exit: 0 ok | 1 parse/consistency error | 2 check drift
"""
import argparse
import datetime as dt
import json
import re
import subprocess
import sys
from pathlib import Path

WORKSTREAM_ID = "COMMUNITY"
REPO = "KritvaOS/kritvaos-community"
MILESTONES = ["KOS-I2", "KOS-I3", "KOS-I4"]
ROW = re.compile(r"^\|\s*(I\d-\d{3})\s*\|(.*)\|\s*$")
SHA = re.compile(r"^[0-9a-f]{40}$")


STATUS = re.compile(r"^(done|in progress|not started|blocked)", re.I)


def parse_tasks(path):
    """Return {task_id: status_text} for one milestone TASKS.md.

    Only rows whose last cell is a recognised status count (the KOS-I4 task
    table has no status column and is skipped; its later status table counts).
    Fails instead of guessing when a task row never gets a status, when a task
    has two status rows, or when no rows are found.
    """
    seen, out = set(), {}
    for line in path.read_text(encoding="utf-8").splitlines():
        m = ROW.match(line)
        if not m:
            continue
        tid = m.group(1)
        seen.add(tid)
        last = m.group(2).split("|")[-1].strip()
        if STATUS.match(last):
            if tid in out:
                raise ValueError(f"{path}: duplicate status row for {tid}")
            out[tid] = last
    if not seen:
        raise ValueError(f"{path}: no task rows found")
    missing = sorted(seen - set(out))
    if missing:
        raise ValueError(f"{path}: no recognised status for {missing}")
    return out


def execution(status_text):
    t = status_text.lower()
    if t.startswith("done"):
        return "COMPLETE"
    if t.startswith("in progress"):
        return "IN_PROGRESS"
    if t.startswith("not started"):
        return "NOT_STARTED"
    if t.startswith("blocked"):
        return "BLOCKED"
    raise ValueError(f"unrecognised task status {status_text!r}")


def git(root, *args):
    try:
        r = subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True)
        return r.stdout.strip() if r.returncode == 0 else None
    except OSError:
        return None


def ref(root, rel):
    p = root / ".git" / rel
    return p.read_text().strip() if p.exists() else None


def source_info(root):
    head = git(root, "rev-parse", "HEAD") or ref(root, "refs/heads/main")
    if not head or not SHA.match(head):
        raise ValueError("cannot determine 40-hex HEAD commit")
    origin = git(root, "rev-parse", "origin/main") or ref(root, "refs/remotes/origin/main")
    dirty = git(root, "status", "--porcelain")
    if origin is None:
        sync = "NOT_SYNCED"
    elif dirty is None:  # git unavailable: cleanliness unverified, so never claim SYNCED
        sync = "COMMITTED"
    elif dirty:  # uncommitted changes present
        sync = "NOT_COMMITTED"
    elif head == origin:
        sync = "SYNCED"
    else:
        sync = "NOT_SYNCED"
    return {"branch": "main", "commit": head, "tag": None}, sync, dirty is None


def build(root, updated_at):
    source, sync, dirty_unchecked = source_info(root)
    tasks = []
    for ms in MILESTONES:
        tpath = root / "planning" / "milestones" / ms / "TASKS.md"
        rows = parse_tasks(tpath)
        if not rows:
            raise ValueError(f"{tpath}: no task rows found")
        for tid in sorted(rows):
            acc = f"planning/milestones/{ms}/{tid}/ACCEPTANCE_CRITERIA.md"
            tasks.append({
                "task_id": tid,
                "execution": execution(rows[tid]),
                "git_sync": sync,
                "readiness": "IMPLEMENTABLE",
                "evidence": [{
                    "type": "software",
                    "uri": acc,
                    "description": f"Criteria reference only (not completion proof); planning status text: {rows[tid]!r}",
                    "commit": None, "ci_run": None, "report_sha256": None, "result": None,
                }],
            })
    doc = {"schema_version": "2.0", "workstream_id": WORKSTREAM_ID, "repo": REPO,
           "source": source, "tasks": tasks, "updated_at": updated_at}
    return doc, dirty_unchecked


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=".")
    ap.add_argument("--out", default="status/status.json")
    ap.add_argument("--updated-at", help="ISO-8601 UTC (default: now)")
    ap.add_argument("--check", action="store_true", help="fail if output file differs from regeneration")
    ap.add_argument("--check-facts", action="store_true",
                    help="fail if planning-derived facts differ (ignores source, git_sync, updated_at)")
    a = ap.parse_args()
    root = Path(a.root)
    ts = a.updated_at or dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    try:
        doc, dirty_unchecked = build(root, ts)
    except (ValueError, OSError) as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1
    if dirty_unchecked:
        print("WARN: git unavailable; uncommitted changes were not checked", file=sys.stderr)
    text = json.dumps(doc, indent=2) + "\n"
    out = root / a.out
    if a.check_facts:
        if not out.exists():
            print(f"ERROR: {out} not found", file=sys.stderr)
            return 2
        old = json.loads(out.read_text())
        strip = lambda d: {**{k: v for k, v in d.items() if k not in ("source", "updated_at")},
                           "tasks": [{k: v for k, v in t.items() if k != "git_sync"} for t in d["tasks"]]}
        if strip(old) != strip(doc):
            print("status.json facts differ from planning records; regenerate with scripts/gen_status.py", file=sys.stderr)
            return 2
        return 0
    if a.check:
        if not out.exists():
            return 2
        old = json.loads(out.read_text())
        old["updated_at"] = doc["updated_at"]
        if old != doc:
            print("status.json out of date", file=sys.stderr)
            return 2
        return 0
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
