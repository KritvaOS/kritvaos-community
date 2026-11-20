#!/usr/bin/env python3
#==============================================================================
# Copyright (c) 2026 Kritva Robotics Private Limited.
# SPDX-License-Identifier: Apache-2.0
#
# File        : check_dco_signoff.py
# Description : Checks that every new commit carries a Developer Certificate of Origin sign-off
#
# Component   : Infrastructure
# Module      : Repository Governance Controls
# Layer       : Development Infrastructure
#
# Requirements: N/A - kritva-program decision D-140 follow-up
# API         : Command-line commit message validation
#
# Author      : KritvaOS
# Created     : 20-11-2026
#==============================================================================

"""Check the DCO sign-off (kritva-program decision D-140; CONTRIBUTING.md section 4).

Every non-merge commit in the range needs one 'Signed-off-by: Name <email>' line whose email matches the
commit's author or committer email (case-insensitive), as `git commit -s` produces.
Commits by GitHub bot accounts (author email ending in '[bot]@users.noreply.github.com') are exempt.
Existing history is never checked or rewritten.

  --range BASE..HEAD   every non-merge commit in the range (used by CI on pull requests)
  --file PATH          one message file, checked for a well-formed sign-off only (commit-msg hook)
Exit 1 on problems, 2 on usage or git errors.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

SIGNOFF = re.compile(r"^Signed-off-by: (\S.*?) <([^<>\s@]+@[^<>\s@]+)>\s*$", re.IGNORECASE | re.MULTILINE)
BOT = re.compile(r"\[bot\]@users\.noreply\.github\.com$", re.IGNORECASE)
HELP = "add one with 'git commit -s' (or 'git commit --amend -s' for the last commit; 'git rebase --signoff' for several)"


def problems(message, author_email=None, committer_email=None):
    found = SIGNOFF.findall(message)
    if not found:
        return [f"missing 'Signed-off-by: Name <email>' line ({HELP})"]
    if author_email is None and committer_email is None:
        return []
    known = {e.lower() for e in (author_email, committer_email) if e}
    if not any(email.lower() in known for _, email in found):
        return ["no Signed-off-by email matches the commit author or committer"]
    return []


def commits(rev_range):
    r = subprocess.run(["git", "log", "--no-merges", "--format=%H%x00%ae%x00%ce%x00%B%x01", rev_range],
                       capture_output=True, text=True)
    if r.returncode:
        print(r.stderr, file=sys.stderr)
        sys.exit(2)
    out = []
    for c in r.stdout.split("\x01"):
        if c.strip():
            sha, ae, ce, body = c.lstrip("\n").split("\x00", 3)
            out.append((sha, ae, ce, body))
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--range")
    g.add_argument("--file")
    a = ap.parse_args(argv)
    bad = 0
    if a.file:
        items = [("message", None, None, Path(a.file).read_text(encoding="utf-8"))]
    else:
        items = [(s[:10], ae, ce, m) for s, ae, ce, m in commits(a.range)]
    checked = 0
    for name, ae, ce, msg in items:
        if ae and BOT.search(ae):
            continue
        checked += 1
        for p in problems(msg, ae, ce):
            print(f"{name}: {p}", file=sys.stderr)
            bad += 1
    if bad:
        return 1
    print(f"DCO sign-off ok ({checked} commit(s) checked)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
