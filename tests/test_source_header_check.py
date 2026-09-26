#!/usr/bin/env python3
#==============================================================================
# Copyright (c) 2026 KritvaOS
# SPDX-License-Identifier: Apache-2.0
#
# File        : test_source_header_check.py
# Description : Unit tests for the KritvaOS source header checker
#
# Component   : Infrastructure
# Module      : Source Header Checker
# Layer       : Development
#
# Requirements: Python 3
# API         : unittest
#
# Author      : KritvaOS
# Created     : 26-09-2026
#==============================================================================

from __future__ import annotations
import importlib.util
import tempfile
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
ROOT = TESTS_DIR.parent
CHECKER = ROOT / "scripts" / "lint" / "check_source_headers.py"
FIXTURES = TESTS_DIR / "source_header_check"

spec = importlib.util.spec_from_file_location("check_source_headers", CHECKER)
checker = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(checker)

class SourceHeaderCheckerTests(unittest.TestCase):
    def test_valid_cpp(self):
        self.assertEqual(checker.validate_file(FIXTURES/"valid"/"sample.cpp", ROOT), [])
    def test_valid_python(self):
        self.assertEqual(checker.validate_file(FIXTURES/"valid"/"sample.py", ROOT), [])
    def test_valid_yaml(self):
        self.assertEqual(checker.validate_file(FIXTURES/"valid"/"sample.yaml", ROOT), [])
    def test_valid_markdown(self):
        # Markdown is intentionally not a tracked-source candidate in v0.3.3.
        self.assertEqual(checker.validate_file(FIXTURES/"valid"/"sample.md", ROOT), [])
    def _validate_fixture(self, fixture_name):
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            p = repo / "fixture.cpp"
            p.write_text((FIXTURES/"invalid"/fixture_name).read_text(encoding="utf-8"), encoding="utf-8")
            return checker.validate_file(p, repo)

    def test_bad_date(self):
        e = self._validate_fixture("bad_date.cpp")
        self.assertTrue(any(x["code"] == "HEADER-002" for x in e))

    def test_missing_field(self):
        e = self._validate_fixture("missing_field.cpp")
        self.assertTrue(any(x["code"] == "HEADER-004" for x in e))

    def test_missing_spdx(self):
        e = self._validate_fixture("missing_spdx.cpp")
        self.assertTrue(any(x["code"] == "HEADER-001" for x in e))
    def test_requirements_exempt(self):
        self.assertEqual(checker.validate_file(FIXTURES/"valid"/"sample_requirements.txt", ROOT), [])
    def test_pre_commit_special_file(self):
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            p = repo / ".githooks" / "pre-commit"
            p.parent.mkdir(parents=True)
            p.write_text((FIXTURES/"valid"/"sample_pre_commit").read_text(encoding="utf-8"), encoding="utf-8")
            self.assertEqual(checker.classify(p, repo), "source")
            self.assertEqual(checker.validate_file(p, repo), [])
    def test_workflow_yaml(self):
        self.assertEqual(checker.validate_file(FIXTURES/"valid"/"sample_workflow.yml", ROOT), [])
    def test_unknown_file_is_skipped(self):
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "example.conf"
            p.write_text("key=value\n", encoding="utf-8")
            self.assertEqual(checker.classify(p, Path(td)), "skip")
            self.assertEqual(checker.validate_file(p, Path(td)), [])

if __name__ == "__main__":
    unittest.main(verbosity=2)
