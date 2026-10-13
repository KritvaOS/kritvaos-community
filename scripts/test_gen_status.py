#!/usr/bin/env python3
#==============================================================================
# Copyright (c) 2026 KritvaOS
# SPDX-License-Identifier: Apache-2.0
#
# File        : test_gen_status.py
# Description : Regression tests for gen_status.py task-table parsing
#
# Component   : Infrastructure
# Module      : Program Status Reporting
# Layer       : Development Infrastructure
#
# Requirements: kritva-program decisions D-009, D-011, D-013
# API         : python3 -m unittest scripts/test_gen_status.py
#
# Author      : KritvaOS
# Created     : 11-10-2026
#==============================================================================
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_status  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {"KOS-I2": 6, "KOS-I3": 7, "KOS-I4": 8}


def parse_text(text):
    with tempfile.TemporaryDirectory() as d:
        p = Path(d) / "TASKS.md"
        p.write_text(text, encoding="utf-8")
        return gen_status.parse_tasks(p)


class RealFiles(unittest.TestCase):
    def test_each_milestone_has_expected_unique_tasks(self):
        for ms, n in EXPECTED.items():
            rows = gen_status.parse_tasks(ROOT / "planning" / "milestones" / ms / "TASKS.md")
            self.assertEqual(len(rows), n, ms)
            prefix = "I" + ms[-1] + "-"
            self.assertTrue(all(t.startswith(prefix) for t in rows), ms)

    def test_i4_uses_status_table_not_safety_column(self):
        rows = gen_status.parse_tasks(ROOT / "planning" / "milestones" / "KOS-I4" / "TASKS.md")
        for tid, text in rows.items():
            self.assertTrue(text.lower().startswith("done"), (tid, text))
            self.assertNotIn("Safety-relevant", text)

    def test_build_emits_21_unique_tasks_and_no_acceptance_fields(self):
        doc, _ = gen_status.build(ROOT, "2026-10-11T00:00:00Z")
        ids = [t["task_id"] for t in doc["tasks"]]
        self.assertEqual(len(ids), 21)
        self.assertEqual(len(set(ids)), 21)
        for t in doc["tasks"]:
            self.assertEqual(set(t), {"task_id", "execution", "git_sync", "readiness", "evidence"})


class CommittedReport(unittest.TestCase):
    def test_committed_status_facts_match_planning_records(self):
        import json
        doc, _ = gen_status.build(ROOT, "2026-10-11T00:00:00Z")
        old = json.loads((ROOT / "status" / "status.json").read_text())
        facts = lambda d: [(t["task_id"], t["execution"], t["evidence"]) for t in d["tasks"]]
        self.assertEqual(facts(old), facts(doc))


class Synthetic(unittest.TestCase):
    def test_every_evidence_target_exists(self):
        doc, _ = gen_status.build(ROOT, "2026-10-11T00:00:00Z")
        for t in doc["tasks"]:
            for e in t["evidence"]:
                if e["uri"].startswith("https://"):  # CI run entry: validated by load_ci_evidence, not a file
                    self.assertTrue(e["commit"] and e["ci_run"], e["uri"])
                    continue
                self.assertTrue((ROOT / e["uri"]).is_file(), e["uri"])

    def test_missing_evidence_target_fails(self):
        import shutil
        with tempfile.TemporaryDirectory() as d:
            r = Path(d)
            shutil.copytree(ROOT / "planning", r / "planning")
            (r / ".git" / "refs" / "heads").mkdir(parents=True)
            (r / ".git" / "refs" / "heads" / "main").write_text("a" * 40)
            (r / "planning/milestones/KOS-I2/I2-001/ACCEPTANCE_CRITERIA.md").unlink()
            with self.assertRaises(ValueError):
                gen_status.build(r, "2026-10-11T00:00:00Z")

    def test_status_column_table(self):
        rows = parse_text("| Task | Status |\n|---|---|\n| I2-001 | Runtime | Done |\n")
        self.assertEqual(rows, {"I2-001": "Done"})

    def test_two_table_layout_status_from_later_table(self):
        text = ("| Task | Name | Safety |\n|---|---|---|\n| I4-001 | A | — |\n"
                "## Status\n| Task | Status |\n|---|---|\n| I4-001 | Done; ok |\n")
        self.assertEqual(parse_text(text), {"I4-001": "Done; ok"})

    def test_missing_status_fails(self):
        with self.assertRaises(ValueError):
            parse_text("| I4-001 | A | — |\n")

    def test_unknown_status_fails_in_execution(self):
        with self.assertRaises(ValueError):
            gen_status.execution("Shipped")

    def test_duplicate_status_rows_fail(self):
        with self.assertRaises(ValueError):
            parse_text("| I2-001 | A | Done |\n| I2-001 | A | Done |\n")

    def test_no_rows_fails(self):
        with self.assertRaises(ValueError):
            parse_text("# nothing here\n")


class CiEvidence(unittest.TestCase):
    A, B = "a" * 40, "b" * 40
    URL = "https://github.com/KritvaOS/kritvaos-community/actions/runs/123"

    def entry(self, **kw):
        e = {"commit": self.A, "task_commit": self.A, "ci_run": 123, "url": self.URL, "workflow": "KritvaOS CI",
             "result": "PASS", "kind": "OWN"}
        e.update(kw)
        return e

    def repo_with(self, entries):
        import json
        import shutil
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        r = Path(d.name)
        shutil.copytree(ROOT / "planning", r / "planning")
        (r / ".git" / "refs" / "heads").mkdir(parents=True)
        (r / ".git" / "refs" / "heads" / "main").write_text("a" * 40)
        if entries is not None:
            (r / "status").mkdir()
            (r / "status" / "evidence.json").write_text(json.dumps({"entries": entries}))
        return r

    def test_no_file_means_no_ci_evidence(self):
        doc, _ = gen_status.build(self.repo_with(None), "2026-10-11T00:00:00Z")
        self.assertTrue(all(len(t["evidence"]) == 1 and t["evidence"][0]["commit"] is None for t in doc["tasks"]))

    def test_valid_entry_is_emitted_as_commit_bound_pass(self):
        doc, _ = gen_status.build(self.repo_with({"I2-001": self.entry()}), "2026-10-11T00:00:00Z")
        t = next(t for t in doc["tasks"] if t["task_id"] == "I2-001")
        self.assertEqual(len(t["evidence"]), 2)
        ev = t["evidence"][1]
        self.assertEqual((ev["commit"], ev["ci_run"], ev["result"], ev["uri"]), (self.A, "123", "PASS", self.URL))
        self.assertEqual(sum(1 for x in doc["tasks"] if len(x["evidence"]) == 2), 1)

    def test_bad_entries_are_rejected(self):
        bad = [("I9-999", self.entry()), ("I2-001", self.entry(result="FAIL")),
               ("I2-001", self.entry(commit="abc123")), ("I2-001", self.entry(ci_run=124)),
               ("I2-001", self.entry(url="https://example.com/runs/123")),
               ("I2-001", self.entry(kind="GUESS")), ("I2-001", self.entry(kind="OWN", commit=self.B)),
               ("I2-001", self.entry(workflow=""))]
        for tid, e in bad:
            with self.assertRaises(ValueError, msg=(tid, e)):
                gen_status.build(self.repo_with({tid: e}), "2026-10-11T00:00:00Z")

    def test_milestone_evidence_may_be_on_a_later_commit(self):
        doc, _ = gen_status.build(self.repo_with({"I2-001": self.entry(kind="MILESTONE", commit=self.B)}),
                                  "2026-10-11T00:00:00Z")
        self.assertEqual(next(t for t in doc["tasks"] if t["task_id"] == "I2-001")["evidence"][1]["commit"], self.B)

    def test_committed_evidence_file_matches_report(self):
        import json
        if not (ROOT / "status" / "evidence.json").is_file():
            self.skipTest("no evidence file")
        doc, _ = gen_status.build(ROOT, "2026-10-11T00:00:00Z")
        n = len(json.loads((ROOT / "status" / "evidence.json").read_text())["entries"])
        self.assertEqual(sum(1 for t in doc["tasks"] if len(t["evidence"]) == 2), n)


if __name__ == "__main__":
    unittest.main()
