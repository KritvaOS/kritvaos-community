#!/usr/bin/env python3
"""Tests for the copyright holder line in scripts/lint/check_source_headers.py (kritva-standard decision D-1)."""
import importlib.util
import re
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
spec = importlib.util.spec_from_file_location("check_source_headers", ROOT / "scripts/lint/check_source_headers.py")
H = importlib.util.module_from_spec(spec)
spec.loader.exec_module(H)

SAMPLE = (HERE / "valid" / "sample.cpp").read_text(encoding="utf-8")
HOLDER = re.compile(r"Copyright \(c\) 2026 (?:KritvaOS|Kritva Robotics Private Limited)\.?")


class HolderLine(unittest.TestCase):
    def check(self, holder):
        text = HOLDER.sub(holder, SAMPLE)
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            p = repo / "fixture.cpp"
            p.write_text(text, encoding="utf-8")
            return H.validate_file(p, repo)

    def rejected(self, holder):
        return any(e["code"] == "HEADER-003" for e in self.check(holder))

    def test_new_holder_is_accepted(self):
        self.assertEqual(self.check("Copyright (c) 2026 Kritva Robotics Private Limited."), [])

    def test_old_holder_is_rejected(self):
        self.assertTrue(self.rejected("Copyright (c) 2026 KritvaOS"))

    def test_other_holder_is_rejected(self):
        self.assertTrue(self.rejected("Copyright (c) 2026 Somebody Else"))

    def test_missing_trailing_period_is_rejected(self):
        self.assertTrue(self.rejected("Copyright (c) 2026 Kritva Robotics Private Limited"))


if __name__ == "__main__":
    unittest.main()
