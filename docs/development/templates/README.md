# KritvaOS Source Header Templates

This directory contains the standard source-file header templates for KritvaOS repositories.

## Standard Date Format

Use:

`DD-MM-YYYY`

Example:

`22-09-2026`

## Copyright

Use:

`Copyright (c) 2026 KritvaOS`

The copyright holder may be updated when the legal structure of the project is finalized.

## SPDX

Use:

`SPDX-License-Identifier: Apache-2.0`

for KritvaOS Community source files distributed under Apache-2.0.

## Templates

- `CPP_HEADER.template` — C/C++ source and header files
- `TEST_HEADER.template` — C/C++ test files
- `PYTHON_HEADER.template` — Python
- `SCRIPT_HEADER.template` — Shell scripts
- `YAML_HEADER.template` — YAML/YML
- `CMAKE_HEADER.template` — CMake
- `RTL_HEADER.template` — Verilog/SystemVerilog RTL
- `DTS_HEADER.template` — Linux Device Tree
- `INTERFACE_HEADER.template` — Protocol/interface/schema definitions
- `MARKDOWN_HEADER.template` — Markdown documentation
- `XML_HEADER.template` — XML
- `DOCKERFILE_HEADER.template` — Dockerfile
- `JSON_GUIDANCE.md` — JSON licensing/header guidance

## Header Policy

Use the smallest applicable template.

Do not add metadata fields that have no meaningful value for a particular file.

Git history and repository tags provide source-control history and software versioning; individual source files should not contain a per-file software version field unless there is a specific interface/schema requirement.
