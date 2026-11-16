#!/usr/bin/env python3
"""Tests for the copyright holder transition in scripts/lint/check_source_headers.py (kritva-standard decision D-1)."""
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


class HolderTransition(unittest.TestCase):
    def check(self, holder):
        text = HOLDER.sub(holder, SAMPLE)
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            p = repo / "fixture.cpp"
            p.write_text(text, encoding="utf-8")
            return H.validate_file(p, repo)

    def test_old_holder_is_accepted(self):
        self.assertEqual(self.check("Copyright (c) 2026 KritvaOS"), [])

    def test_new_holder_is_accepted(self):
        self.assertEqual(self.check("Copyright (c) 2026 Kritva Robotics Private Limited."), [])

    def test_other_holder_is_rejected(self):
        errs = self.check("Copyright (c) 2026 Somebody Else")
        self.assertTrue(any(e["code"] == "HEADER-003" for e in errs))


if __name__ == "__main__":
    unittest.main()
