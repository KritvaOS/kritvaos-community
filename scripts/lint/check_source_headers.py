#!/usr/bin/env python3
#==============================================================================
# Copyright (c) 2026 KritvaOS
# SPDX-License-Identifier: Apache-2.0
#
# File        : check_source_headers.py
# Description : Validate KritvaOS source-file headers
#
# Component   : Infrastructure
# Module      : Source Header Checker
# Layer       : Development
#
# Requirements: Python 3
# API         : Command-line interface
#
# Author      : KritvaOS
# Created     : 26-09-2026
#==============================================================================

from __future__ import annotations

import argparse
import datetime as dt
import fnmatch
import json
import subprocess
from pathlib import Path

REQUIRED_FIELDS = (
    "File", "Description", "Component", "Module", "Layer",
    "Requirements", "API", "Author", "Created",
)
SPDX_MARKER = "SPDX-License-Identifier: Apache-2.0"
COPYRIGHT_MARKER = "Copyright (c) 2026 KritvaOS"

EXCLUDED_DIRS = {
    ".git", "build", "out", "third_party", "vendor",
    "generated", "external", "__pycache__",
}
EXCLUDED_FILES = {"LICENSE", "NOTICE", "CHANGELOG.md"}

SOURCE_EXTENSIONS = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx",
    ".py", ".sh", ".bash", ".v", ".sv", ".svh", ".vh", ".dts", ".dtsi",
}
COMMENT_STYLE = {
    ".c": "//", ".cc": "//", ".cpp": "//", ".cxx": "//",
    ".h": "//", ".hh": "//", ".hpp": "//", ".hxx": "//",
    ".py": "#", ".sh": "#", ".bash": "#",
    ".v": "//", ".sv": "//", ".svh": "//", ".vh": "//",
    ".dts": "//", ".dtsi": "//",
}
SPECIAL_FILES = {".githooks/pre-commit": "#"}
CONFIG_FILES = {".github/workflows/*.yml", ".github/workflows/*.yaml"}
EXEMPT_NAMES = {"requirements.txt", "source_header_check.version"}
EXEMPT_PATTERNS = {"*.template", "*.instructions.md"}
EXEMPT_DIRS = {
    ".github/agents",
    ".github/instructions",
    "docs/development/templates",
    "tests/source_header_check",
}
JSON_SUFFIXES = {".json"}

def git_tracked_files(repo: Path) -> list[Path]:
    result = subprocess.run(
        ["git", "-C", str(repo), "ls-files", "-z"],
        check=True, capture_output=True,
    )
    return [repo / p for p in result.stdout.decode("utf-8", errors="replace").split("\0") if p]

def rel(path: Path, repo: Path) -> str:
    return path.relative_to(repo).as_posix()

def is_excluded_dir(path: Path, repo: Path) -> bool:
    parts = path.relative_to(repo).parts
    return any(p in EXCLUDED_DIRS for p in parts)

def is_explicit_exempt(path: Path, repo: Path) -> bool:
    r = rel(path, repo)
    if path.name in EXCLUDED_FILES or path.name in EXEMPT_NAMES:
        return True
    if any(r == d or r.startswith(d + "/") for d in EXEMPT_DIRS):
        return True
    if any(fnmatch.fnmatch(path.name, pat) for pat in EXEMPT_PATTERNS):
        return True
    if path.suffix.lower() in JSON_SUFFIXES:
        return True
    return False

def is_workflow(path: Path, repo: Path) -> bool:
    r = rel(path, repo)
    return any(fnmatch.fnmatch(r, p) for p in CONFIG_FILES)

def classify(path: Path, repo: Path) -> str:
    r = rel(path, repo)
    if is_excluded_dir(path, repo) or is_explicit_exempt(path, repo):
        return "skip"
    if r in SPECIAL_FILES:
        return "source"
    if is_workflow(path, repo):
        return "source"
    if path.suffix.lower() in SOURCE_EXTENSIONS:
        return "source"
    return "skip"

def read_lines(path: Path, count: int = 100) -> list[str]:
    with path.open("r", encoding="utf-8", errors="strict") as f:
        return [line.rstrip("\n") for _, line in zip(range(count), f)]

def extract_header(path: Path, repo: Path) -> tuple[list[str], str | None]:
    r = rel(path, repo)
    lines = read_lines(path)
    start_idx = 1 if lines and lines[0].startswith("#!") else 0

    if r in SPECIAL_FILES:
        marker = "#"
    elif is_workflow(path, repo):
        marker = "#"
    else:
        marker = COMMENT_STYLE.get(path.suffix.lower())
        if marker is None:
            return [], None

    start = next(
        (i for i in range(start_idx, len(lines))
         if lines[i].lstrip().startswith(marker)),
        None,
    )
    if start is None:
        return [], "HEADER-008"

    header = []
    for line in lines[start:]:
        stripped = line.lstrip()
        if stripped.startswith(marker):
            header.append(line)
        elif not stripped:
            if header:
                header.append(line)
        else:
            break
    return header, None

def validate_file(path: Path, repo: Path) -> list[dict]:
    if classify(path, repo) != "source":
        return []

    try:
        header, extraction_error = extract_header(path, repo)
    except UnicodeDecodeError:
        return [{"file": str(path), "code": "HEADER-006",
                 "message": "file is not valid UTF-8 text"}]

    if extraction_error:
        return [{"file": str(path), "code": extraction_error,
                 "message": "missing or malformed leading header block"}]

    text = "\n".join(header)
    errors = []

    if SPDX_MARKER not in text:
        errors.append({"file": str(path), "code": "HEADER-001",
                       "message": "missing SPDX-License-Identifier: Apache-2.0"})
    if COPYRIGHT_MARKER not in text:
        errors.append({"file": str(path), "code": "HEADER-003",
                       "message": "missing Copyright (c) 2026 KritvaOS"})

    for field in REQUIRED_FIELDS:
        if field == "Created":
            continue
        if not any(field in line for line in header):
            errors.append({"file": str(path), "code": "HEADER-004",
                           "message": f"missing or empty required field '{field}'"})

    created_line = next((line for line in header if "Created" in line), None)
    if created_line is None:
        errors.append({"file": str(path), "code": "HEADER-002",
                       "message": "missing 'Created : DD-MM-YYYY'"})
    else:
        value = created_line.split(":", 1)[1].strip() if ":" in created_line else ""
        try:
            dt.datetime.strptime(value, "%d-%m-%Y")
        except ValueError:
            errors.append({"file": str(path), "code": "HEADER-002",
                           "message": "invalid 'Created' date; expected DD-MM-YYYY"})
    return errors

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("tracked", "files"), default="tracked")
    parser.add_argument("--files", nargs="*")
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--strict-rules", action="store_true")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    repo = Path.cwd().resolve()
    paths = (git_tracked_files(repo) if args.mode == "tracked"
             else [(repo / p).resolve() for p in (args.files or [])])

    scanned = 0
    checked = 0
    skipped = 0
    errors = []

    for path in paths:
        if not path.is_file():
            continue
        scanned += 1
        classification = classify(path, repo)
        if classification == "source":
            checked += 1
            errors.extend(validate_file(path, repo))
        else:
            skipped += 1

    if args.json:
        print(json.dumps({
            "scanned": scanned, "checked": checked,
            "skipped": skipped, "errors": errors
        }, indent=2))
    else:
        print(f"[header-check] scanned {scanned} tracked/file(s)")
        print(f"[header-check] checked {checked} source/config file(s)")
        print(f"[header-check] skipped {skipped} file(s)")
        if errors:
            print("[header-check] FAILED")
            for e in errors:
                print(f"  {e['file']}: [{e['code']}] {e['message']}")
        else:
            print("[header-check] PASSED")

    return 1 if errors else 0

if __name__ == "__main__":
    raise SystemExit(main())
