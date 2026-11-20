#!/usr/bin/env python3
#==============================================================================
# Copyright (c) 2026 Kritva Robotics Private Limited.
# SPDX-License-Identifier: Apache-2.0
#
# File        : test_check_dco_signoff.py
# Description : Unit tests for the DCO sign-off checker
#
# Component   : Infrastructure
# Module      : Repository Governance Controls
# Layer       : Development Infrastructure
#
# Requirements: N/A - kritva-program decision D-140 follow-up
# API         : unittest
#
# Author      : KritvaOS
# Created     : 20-11-2026
#==============================================================================

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts" / "security"))
import check_dco_signoff as dco  # noqa: E402


class MessageTests(unittest.TestCase):
    def test_signed_message_passes(self):
        m = "docs: x\n\nbody\n\nTasks: None (r)\nSigned-off-by: Ann Lee <ann@x.io>\nCo-Authored-By: Bot <b@x.io>"
        self.assertEqual(dco.problems(m), [])

    def test_missing_signoff_is_rejected(self):
        self.assertTrue(dco.problems("docs: x\n\nTasks: None (r)"))

    def test_malformed_signoff_is_rejected(self):
        self.assertTrue(dco.problems("docs: x\n\nSigned-off-by: Ann Lee"))
        self.assertTrue(dco.problems("docs: x\n\nSigned-off-by: <ann@x.io>"))

    def test_signoff_must_match_author_or_committer(self):
        m = "docs: x\n\nSigned-off-by: Ann <ann@x.io>"
        self.assertEqual(dco.problems(m, "ANN@x.io", "web@gh.io"), [])
        self.assertEqual(dco.problems(m, "other@x.io", "ann@x.io"), [])
        self.assertTrue(dco.problems(m, "other@x.io", "web@gh.io"))


class RangeTests(unittest.TestCase):
    def git(self, d, *args, email="t@t.t"):
        subprocess.run(["git", "-C", d, *args], check=True, capture_output=True,
                       env={**os.environ, "GIT_AUTHOR_NAME": "t", "GIT_AUTHOR_EMAIL": email,
                            "GIT_COMMITTER_NAME": "t", "GIT_COMMITTER_EMAIL": email})

    def run_range(self, d):
        cwd = os.getcwd()
        os.chdir(d)
        try:
            return dco.main(["--range", "HEAD~1..HEAD"])
        finally:
            os.chdir(cwd)

    def test_range_checks_only_new_commits(self):
        with tempfile.TemporaryDirectory() as d:
            self.git(d, "init", "-q")
            self.git(d, "commit", "-q", "--allow-empty", "-m", "old unsigned")
            self.git(d, "commit", "-q", "--allow-empty", "-s", "-m", "new signed")
            self.assertEqual(self.run_range(d), 0)

    def test_unsigned_new_commit_fails(self):
        with tempfile.TemporaryDirectory() as d:
            self.git(d, "init", "-q")
            self.git(d, "commit", "-q", "--allow-empty", "-m", "old")
            self.git(d, "commit", "-q", "--allow-empty", "-m", "new unsigned")
            self.assertEqual(self.run_range(d), 1)

    def test_bot_commits_are_exempt(self):
        with tempfile.TemporaryDirectory() as d:
            self.git(d, "init", "-q")
            self.git(d, "commit", "-q", "--allow-empty", "-m", "old")
            self.git(d, "commit", "-q", "--allow-empty", "-m", "bump", email="1+dependabot[bot]@users.noreply.github.com")
            self.assertEqual(self.run_range(d), 0)


if __name__ == "__main__":
    unittest.main()
