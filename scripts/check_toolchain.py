#!/usr/bin/env python3
#==============================================================================
# Copyright (c) 2026 KritvaOS
# SPDX-License-Identifier: Apache-2.0
#
# File        : check_toolchain.py
# Description : Validates the KritvaOS development toolchain configuration
#
# Component   : Infrastructure
# Module      : Toolchain Validation
# Layer       : Development Infrastructure
#
# Requirements: TOOLCHAIN-001
# API         : Command-line toolchain validation
#
# Author      : KritvaOS
# Created     : 26-09-2026
#==============================================================================
"""
Kritva Toolchain Validator

Validates the active development environment against:
    toolchain/VERSIONS.yaml

Design goals:
    - Keep VERSIONS.yaml as the single source of truth.
    - Normalize installed versions for clean human/JSON output.
    - Validate minimum versions and optionally enforce recommended versions.
    - Support simple list profiles and inherited structured profiles.
    - Support Python package checks.
    - Detect configuration errors separately from tool failures.
    - Remain compatible with the Kritva v0.1 toolchain schema.

Usage:
    python3 scripts/check_toolchain.py
    python3 scripts/check_toolchain.py --profile default
    python3 scripts/check_toolchain.py --profile ci
    python3 scripts/check_toolchain.py --profile soc
    python3 scripts/check_toolchain.py --profile full

Strict mode:
    python3 scripts/check_toolchain.py --profile ci --strict

Machine-readable output:
    python3 scripts/check_toolchain.py --profile ci --json

Exit codes:
    0 = validation successful
    1 = one or more validation failures
    2 = configuration / invocation error
"""

from __future__ import annotations

import argparse
import json
import platform
import re
import shutil
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

try:
    import yaml
except ImportError:
    print(
        "ERROR: PyYAML is required.\n"
        "Install with:\n"
        "  python3 -m pip install pyyaml",
        file=sys.stderr,
    )
    sys.exit(2)


# ============================================================================
# Paths
# ============================================================================

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
VERSIONS_FILE = REPO_ROOT / "toolchain" / "VERSIONS.yaml"


# ============================================================================
# Constants
# ============================================================================

STATUS_PASS = "PASS"
STATUS_WARN = "WARN"
STATUS_FAIL = "FAIL"
STATUS_SKIP = "SKIP"

# The first entry is the canonical location in the current Kritva schema.
# Additional roots allow future VERSIONS.yaml revisions to place domain tools
# outside the "toolchain" mapping without requiring another validator rewrite.
TOOL_DEFINITION_ROOTS = (
    "toolchain",
    "simulation",
    "hardware",
    "robotics",
    "ai",
    "documentation",
    "quality",
)

VERSION_PATTERN = re.compile(r"(?<!\d)(\d+)(?:\.(\d+))?(?:\.(\d+))?(?:\.(\d+))?")

# Commands are deliberately explicit.  They are the executable probes used by
# the validator, not the authoritative version values.
TOOL_COMMANDS: dict[str, list[list[str]]] = {
    "cmake": [["cmake", "--version"]],
    "ninja": [["ninja", "--version"]],
    "clang": [["clang", "--version"]],
    "clang_format": [
        ["clang-format", "--version"],
        ["clang-format-20", "--version"],
        ["clang-format-19", "--version"],
        ["clang-format-18", "--version"],
    ],
    "clang_tidy": [
        ["clang-tidy", "--version"],
        ["clang-tidy-20", "--version"],
        ["clang-tidy-19", "--version"],
        ["clang-tidy-18", "--version"],
    ],
    "gcc": [["gcc", "--version"]],
    "python": [["python3", "--version"], ["python", "--version"]],
    "pip": [["python3", "-m", "pip", "--version"]],
    "git": [["git", "--version"]],
    "docker": [["docker", "--version"]],
    "renode": [["renode", "--version"]],
    "verilator": [["verilator", "--version"]],
    "iverilog": [["iverilog", "-V"]],
    "vivado": [["vivado", "-version"]],
    "vitis": [["vitis", "-version"]],
    "quartus": [["quartus", "--version"]],
    "riscv_gnu_toolchain": [["riscv64-unknown-elf-gcc", "--version"]],
    "arm_gnu_toolchain": [["arm-none-eabi-gcc", "--version"]],
    "doxygen": [["doxygen", "--version"]],
    "sphinx": [["sphinx-build", "--version"]],
    "cppcheck": [["cppcheck", "--version"]],
    "gcov": [["gcov", "--version"]],
    "llvm_cov": [["llvm-cov", "--version"]],
    "ros2": [["ros2", "--version"]],
    "ros2_control": [["ros2", "control", "--help"]],
    "micro_ros": [["micro-ros", "--help"]],
    "ethercat": [["ethercat", "--version"]],
    "pytorch": [],
    "onnx": [],
    "onnxruntime": [],
}


# ============================================================================
# Result Model
# ============================================================================


@dataclass
class ToolResult:
    name: str
    required: bool
    installed: bool
    installed_version: str | None
    minimum_version: str | None
    recommended_version: str | None
    status: str
    message: str


# ============================================================================
# Version Handling
# ============================================================================


def parse_version(value: str | None) -> tuple[int, ...] | None:
    """
    Convert a version specification into a comparable tuple.

    Examples:
        "3.12.3"       -> (3, 12, 3)
        "3.12"         -> (3, 12)
        "cmake 3.31.5" -> (3, 31, 5)
        "18.x"         -> (18,)

    Returns None when no numeric version is present.
    """
    if not value:
        return None

    match = VERSION_PATTERN.search(str(value))
    if not match:
        return None

    return tuple(int(group) for group in match.groups() if group is not None)


def version_at_least(
    installed: tuple[int, ...] | None,
    minimum: tuple[int, ...] | None,
) -> bool:
    if installed is None or minimum is None:
        return False

    length = max(len(installed), len(minimum))
    left = installed + (0,) * (length - len(installed))
    right = minimum + (0,) * (length - len(minimum))
    return left >= right


def version_matches_recommended(
    installed: str | None,
    recommended: str | None,
) -> bool:
    """
    Match supported wildcard specifications such as:
        18.x
        3.12.x
        1.11.x

    TBD means "not currently constrained".
    """
    if not installed or not recommended:
        return False

    if str(recommended).upper() == "TBD":
        return True

    installed_version = parse_version(installed)
    if installed_version is None:
        return False

    parts = str(recommended).split(".")

    for index, part in enumerate(parts):
        if part.lower() == "x":
            return True

        try:
            expected = int(part)
        except ValueError:
            return False

        if index >= len(installed_version):
            return False

        if installed_version[index] != expected:
            return False

    return True


def extract_version(output: str | None) -> str | None:
    """
    Extract the most useful semantic version from command output.

    The old implementation returned the complete first output line.  That
    caused output such as:

        Ubuntu clang version
        Ubuntu clang-format v

    to appear in the report.

    We instead locate numeric semantic-version tokens and return the first
    meaningful token from the command output.
    """
    if not output:
        return None

    # Prefer a conventional "version <number>" / "v<number>" occurrence.
    explicit = re.search(
        r"(?:\bversion\s+|[-\s]v)(\d+(?:\.\d+){1,3})\b",
        output,
        flags=re.IGNORECASE,
    )
    if explicit:
        return explicit.group(1)

    # Fall back to the first semantic-looking version.
    match = re.search(r"(?<!\d)\d+\.\d+(?:\.\d+){0,2}(?!\d)", output)
    if match:
        return match.group(0)

    return None


# ============================================================================
# Command Execution
# ============================================================================


def command_exists(command: str) -> bool:
    return shutil.which(command) is not None


def run_command(
    command: list[str],
    timeout: int = 10,
) -> tuple[bool, str]:
    """Execute a command and return (success, combined_output)."""
    try:
        result = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return False, str(exc)

    return result.returncode == 0, result.stdout.strip()


# ============================================================================
# Tool Version Detection
# ============================================================================


def detect_python_package(package_name: str) -> tuple[bool, str | None]:
    """
    Detect a Python package using the interpreter running this validator.

    Package names are mapped to importlib.metadata distribution names.
    """
    module_name = {
        "pytorch": "torch",
        "onnx": "onnx",
        "onnxruntime": "onnxruntime",
    }.get(package_name, package_name)

    command = [
        sys.executable,
        "-c",
        (
            "import importlib.metadata as m; "
            f"print(m.version({module_name!r}))"
        ),
    ]

    success, output = run_command(command)
    if not success or not output:
        return False, None

    return True, extract_version(output) or output.strip()


def detect_tool_version(tool_name: str) -> tuple[bool, str | None]:
    """
    Detect whether a tool exists and return a normalized version string.

    Python packages are handled separately.
    """
    commands = TOOL_COMMANDS.get(tool_name)

    if commands is None:
        # If the tool is not in the built-in command map, try the tool name
        # itself. This makes simple future command-backed tools usable without
        # modifying this file immediately.
        if not command_exists(tool_name):
            return False, None

        commands = [[tool_name, "--version"]]

    if not commands:
        return detect_python_package(tool_name)

    for command in commands:
        executable = command[0]

        if not command_exists(executable) and executable != sys.executable:
            continue

        success, output = run_command(command)

        # Some tools return non-zero for version/help probes while still
        # producing useful version information.
        if output:
            version = extract_version(output)
            if version:
                return True, version

        if success and output:
            return True, output.splitlines()[0].strip()

    return False, None


# ============================================================================
# YAML Helpers
# ============================================================================


def load_versions_file() -> dict[str, Any]:
    if not VERSIONS_FILE.exists():
        raise FileNotFoundError(
            f"Toolchain definition not found: {VERSIONS_FILE}"
        )

    with VERSIONS_FILE.open("r", encoding="utf-8") as file:
        data = yaml.safe_load(file)

    if not isinstance(data, dict):
        raise ValueError(
            f"Invalid YAML root in {VERSIONS_FILE}; expected mapping."
        )

    return data


def flatten_tools(data: dict[str, Any]) -> dict[str, dict[str, Any]]:
    """
    Flatten tool definitions from supported YAML roots.

    Current v0.1 definitions live below:
        toolchain:
            ...

    Future domain-specific definitions can also live below:
        simulation:
        hardware:
        robotics:
        ai:
        documentation:
        quality:

    A mapping is treated as a tool definition when it contains one or more
    recognized declaration fields.
    """
    flattened: dict[str, dict[str, Any]] = {}

    definition_fields = {
        "required",
        "minimum",
        "recommended",
        "version",
        "distribution",
        "implementation",
        "command",
        "package",
    }

    def walk(node: Any) -> None:
        if not isinstance(node, dict):
            return

        for key, value in node.items():
            if not isinstance(value, dict):
                continue

            if any(field in value for field in definition_fields):
                flattened[key] = value
                continue

            walk(value)

    for root_name in TOOL_DEFINITION_ROOTS:
        root = data.get(root_name)
        if isinstance(root, dict):
            walk(root)

    return flattened


# ============================================================================
# Profile Resolution
# ============================================================================


def resolve_profile(
    profiles: dict[str, Any],
    profile_name: str,
    stack: list[str] | None = None,
) -> list[str]:
    """
    Resolve a Kritva toolchain profile.

    Supported forms:

        default:
          - cmake
          - ninja

        soc:
          extends: ci
          additional:
            - renode
            - verilator
    """
    if stack is None:
        stack = []

    if profile_name in stack:
        cycle = " -> ".join(stack + [profile_name])
        raise ValueError(
            f"Circular profile inheritance detected: {cycle}"
        )

    if profile_name not in profiles:
        raise ValueError(
            f"Unknown toolchain profile: {profile_name}"
        )

    profile = profiles[profile_name]

    if isinstance(profile, list):
        return list(dict.fromkeys(profile))

    if not isinstance(profile, dict):
        raise ValueError(
            f"Invalid profile definition: {profile_name}"
        )

    result: list[str] = []

    extends = profile.get("extends")
    if extends:
        if not isinstance(extends, str):
            raise ValueError(
                f"Profile '{profile_name}' has a non-string 'extends'."
            )
        result.extend(
            resolve_profile(
                profiles,
                extends,
                stack + [profile_name],
            )
        )

    for field in ("additional", "tools"):
        entries = profile.get(field, [])
        if entries is None:
            continue
        if not isinstance(entries, list):
            raise ValueError(
                f"Profile '{profile_name}' field '{field}' must be a list."
            )

        for tool in entries:
            if not isinstance(tool, str):
                raise ValueError(
                    f"Profile '{profile_name}' contains a non-string tool."
                )
            if tool not in result:
                result.append(tool)

    return result


def resolve_all_profiles(
    profiles: dict[str, Any],
) -> dict[str, list[str]]:
    return {
        name: resolve_profile(profiles, name)
        for name in profiles
    }


# ============================================================================
# Validation
# ============================================================================


def validate_tool(
    tool_name: str,
    definition: dict[str, Any],
    strict: bool,
) -> ToolResult:
    required = bool(definition.get("required", False))
    minimum = definition.get("minimum")
    recommended = definition.get("recommended")

    # A declaration of version: TBD means validation is intentionally deferred.
    declared_version = definition.get("version")
    if str(declared_version).upper() == "TBD":
        return ToolResult(
            name=tool_name,
            required=required,
            installed=False,
            installed_version=None,
            minimum_version=str(minimum) if minimum is not None else None,
            recommended_version=(
                str(recommended) if recommended is not None else None
            ),
            status=STATUS_SKIP,
            message="Version is TBD; validation deferred.",
        )

    installed, version = detect_tool_version(tool_name)

    minimum_text = str(minimum) if minimum is not None else None
    recommended_text = (
        str(recommended) if recommended is not None else None
    )

    if not installed:
        if required:
            return ToolResult(
                name=tool_name,
                required=True,
                installed=False,
                installed_version=None,
                minimum_version=minimum_text,
                recommended_version=recommended_text,
                status=STATUS_FAIL,
                message="Required tool is not installed or version could not be detected.",
            )

        return ToolResult(
            name=tool_name,
            required=False,
            installed=False,
            installed_version=None,
            minimum_version=minimum_text,
            recommended_version=recommended_text,
            status=STATUS_SKIP,
            message="Optional tool is not installed or version could not be detected.",
        )

    installed_parsed = parse_version(version)

    if minimum_text:
        minimum_parsed = parse_version(minimum_text)

        if minimum_parsed is None:
            return ToolResult(
                name=tool_name,
                required=required,
                installed=True,
                installed_version=version,
                minimum_version=minimum_text,
                recommended_version=recommended_text,
                status=STATUS_FAIL,
                message=f"Invalid minimum version specification: {minimum_text}.",
            )

        if installed_parsed is None:
            return ToolResult(
                name=tool_name,
                required=required,
                installed=True,
                installed_version=version,
                minimum_version=minimum_text,
                recommended_version=recommended_text,
                status=STATUS_FAIL,
                message="Installed version could not be parsed.",
            )

        if not version_at_least(installed_parsed, minimum_parsed):
            return ToolResult(
                name=tool_name,
                required=required,
                installed=True,
                installed_version=version,
                minimum_version=minimum_text,
                recommended_version=recommended_text,
                status=STATUS_FAIL,
                message=(
                    f"Installed version {version} does not satisfy "
                    f"minimum version {minimum_text}."
                ),
            )

    if recommended_text and recommended_text.upper() != "TBD":
        if not version_matches_recommended(version, recommended_text):
            if strict:
                return ToolResult(
                    name=tool_name,
                    required=required,
                    installed=True,
                    installed_version=version,
                    minimum_version=minimum_text,
                    recommended_version=recommended_text,
                    status=STATUS_FAIL,
                    message=(
                        f"Installed version {version} differs from "
                        f"recommended version {recommended_text}."
                    ),
                )

            return ToolResult(
                name=tool_name,
                required=required,
                installed=True,
                installed_version=version,
                minimum_version=minimum_text,
                recommended_version=recommended_text,
                status=STATUS_WARN,
                message=(
                    f"Installed version {version} differs from "
                    f"recommended version {recommended_text}."
                ),
            )

    return ToolResult(
        name=tool_name,
        required=required,
        installed=True,
        installed_version=version,
        minimum_version=minimum_text,
        recommended_version=recommended_text,
        status=STATUS_PASS,
        message="Version requirements satisfied.",
    )


def validate_python_environment(
    definition: dict[str, Any],
    strict: bool,
) -> ToolResult:
    """Validate the Python interpreter running this script."""
    required = bool(definition.get("required", True))
    minimum = str(definition.get("minimum", "3.12.0"))
    recommended = definition.get("recommended")
    recommended = str(recommended) if recommended is not None else None

    installed = platform.python_version()

    installed_parsed = parse_version(installed)
    minimum_parsed = parse_version(minimum)

    if minimum_parsed is None:
        return ToolResult(
            name="python",
            required=required,
            installed=True,
            installed_version=installed,
            minimum_version=minimum,
            recommended_version=recommended,
            status=STATUS_FAIL,
            message=f"Invalid Python minimum version: {minimum}.",
        )

    if installed_parsed is None or not version_at_least(
        installed_parsed,
        minimum_parsed,
    ):
        return ToolResult(
            name="python",
            required=required,
            installed=True,
            installed_version=installed,
            minimum_version=minimum,
            recommended_version=recommended,
            status=STATUS_FAIL,
            message=(
                f"Python {installed} does not satisfy "
                f"minimum version {minimum}."
            ),
        )

    if recommended and recommended.upper() != "TBD":
        if not version_matches_recommended(installed, recommended):
            status = STATUS_FAIL if strict else STATUS_WARN
            return ToolResult(
                name="python",
                required=required,
                installed=True,
                installed_version=installed,
                minimum_version=minimum,
                recommended_version=recommended,
                status=status,
                message=(
                    f"Installed Python {installed} differs from "
                    f"recommended version {recommended}."
                ),
            )

    return ToolResult(
        name="python",
        required=required,
        installed=True,
        installed_version=installed,
        minimum_version=minimum,
        recommended_version=recommended,
        status=STATUS_PASS,
        message="Python version requirement satisfied.",
    )


# ============================================================================
# Output
# ============================================================================


def print_text_report(
    profile: str,
    results: list[ToolResult],
) -> None:
    print()
    print("=" * 78)
    print("Kritva Toolchain Validation")
    print("=" * 78)
    print(f"Repository : {REPO_ROOT}")
    print(f"Definition : {VERSIONS_FILE}")
    print(f"Profile    : {profile}")
    print("=" * 78)
    print()

    print(
        f"{'Tool':24} "
        f"{'Status':8} "
        f"{'Installed':14} "
        f"{'Minimum':14} "
        f"{'Recommended':14}"
    )
    print("-" * 78)

    for result in results:
        installed = result.installed_version or "-"
        minimum = result.minimum_version or "-"
        recommended = result.recommended_version or "-"

        print(
            f"{result.name:24} "
            f"{result.status:8} "
            f"{installed:14} "
            f"{minimum:14} "
            f"{recommended:14}"
        )

    print()
    print("-" * 78)

    failures = [r for r in results if r.status == STATUS_FAIL]
    warnings = [r for r in results if r.status == STATUS_WARN]
    passed = [r for r in results if r.status == STATUS_PASS]
    skipped = [r for r in results if r.status == STATUS_SKIP]

    print(f"PASS : {len(passed)}")
    print(f"WARN : {len(warnings)}")
    print(f"FAIL : {len(failures)}")
    print(f"SKIP : {len(skipped)}")

    for result in results:
        if result.status in {STATUS_WARN, STATUS_FAIL}:
            print(
                f"[{result.status}] "
                f"{result.name}: "
                f"{result.message}"
            )

    print()

    if failures:
        print("Toolchain validation FAILED.")
    elif warnings:
        print("Toolchain validation PASSED with warnings.")
    else:
        print("Toolchain validation PASSED.")

    print()


def print_json_report(
    profile: str,
    results: list[ToolResult],
) -> None:
    output = {
        "schema_version": 1,
        "profile": profile,
        "repository": str(REPO_ROOT),
        "definition": str(VERSIONS_FILE),
        "results": [asdict(result) for result in results],
        "summary": {
            "pass": sum(r.status == STATUS_PASS for r in results),
            "warn": sum(r.status == STATUS_WARN for r in results),
            "fail": sum(r.status == STATUS_FAIL for r in results),
            "skip": sum(r.status == STATUS_SKIP for r in results),
        },
    }

    print(json.dumps(output, indent=2))


# ============================================================================
# Main
# ============================================================================


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate the Kritva development toolchain."
    )

    parser.add_argument(
        "--profile",
        default="default",
        help="Toolchain profile to validate (default: default).",
    )

    parser.add_argument(
        "--strict",
        action="store_true",
        help="Treat recommended-version mismatches as failures.",
    )

    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit machine-readable JSON output.",
    )

    parser.add_argument(
        "--list-profiles",
        action="store_true",
        help="List available toolchain profiles.",
    )

    args = parser.parse_args()

    try:
        data = load_versions_file()
    except (OSError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    profiles = data.get("profiles", {})
    if not isinstance(profiles, dict):
        print(
            "ERROR: 'profiles' must be a mapping.",
            file=sys.stderr,
        )
        return 2

    try:
        if args.list_profiles:
            resolved = resolve_all_profiles(profiles)
            for name, tools in resolved.items():
                print(f"{name}:")
                for tool in tools:
                    print(f"  - {tool}")
            return 0

        selected_tools = resolve_profile(
            profiles,
            args.profile,
        )
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    tools = flatten_tools(data)

    results: list[ToolResult] = []

    for tool_name in selected_tools:
        definition = tools.get(tool_name)

        if definition is None:
            results.append(
                ToolResult(
                    name=tool_name,
                    required=True,
                    installed=False,
                    installed_version=None,
                    minimum_version=None,
                    recommended_version=None,
                    status=STATUS_FAIL,
                    message=(
                        "Tool is referenced by profile but is not "
                        "defined in the toolchain configuration."
                    ),
                )
            )
            continue

        if tool_name == "python":
            results.append(
                validate_python_environment(
                    definition,
                    args.strict,
                )
            )
            continue

        results.append(
            validate_tool(
                tool_name,
                definition,
                args.strict,
            )
        )

    if args.json:
        print_json_report(args.profile, results)
    else:
        print_text_report(args.profile, results)

    if any(result.status == STATUS_FAIL for result in results):
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
