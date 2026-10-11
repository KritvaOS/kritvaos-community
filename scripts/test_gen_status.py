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


if __name__ == "__main__":
    unittest.main()
