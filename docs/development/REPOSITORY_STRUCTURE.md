<!--
Copyright (c) 2026 KritvaOS
SPDX-License-Identifier: Apache-2.0

File        : REPOSITORY_STRUCTURE.md
Description : Canonical KritvaOS repository structure and file responsibilities.

Component   : KritvaOS
Module      : Development Documentation
Layer       : Documentation

Author      : KritvaOS Development Team
Created     : 25-09-2026
-->

# KritvaOS Repository Structure and File Responsibility

**Status:** Proposed Standard — KritvaOS v0.1  
**Canonical Repository:** `KritvaOS`  
**Recommended filename:** `REPOSITORY_STRUCTURE.md`  
**Recommended location:** `docs/development/REPOSITORY_STRUCTURE.md`

---

## 1. Purpose

This document defines the canonical folder structure of the main `KritvaOS` repository and the responsibility of each major folder and important repository-level file.

The objective is to:

- keep the repository easy to understand;
- separate product architecture from implementation;
- provide a consistent structure for interns and contributors;
- avoid duplication between the main repository and component repositories;
- define where development standards, tooling, CI, documentation, and source code belong; and
- support future integration of independently versioned component repositories.

---

## 2. Recommended Main Repository Structure

```text
KritvaOS/
├── .github/
│   ├── workflows/
│   ├── ISSUE_TEMPLATE/
│   ├── PULL_REQUEST_TEMPLATE.md
│   ├── CODEOWNERS
│   └── ...
│
├── .devcontainer/
│   ├── Dockerfile
│   └── devcontainer.json
│
├── cmake/
│   └── ...
│
├── docs/
│   ├── architecture/
│   │   ├── ARCHITECTURE.md
│   │   ├── SYSTEM_ARCHITECTURE.md
│   │   └── ...
│   │
│   ├── development/
│   │   ├── REPOSITORY_STRUCTURE.md
│   │   ├── CONTRIBUTION_GUIDELINES.md
│   │   ├── templates/
│   │   │   ├── CPP_HEADER.template
│   │   │   ├── TEST_HEADER.template
│   │   │   ├── PYTHON_HEADER.template
│   │   │   ├── SCRIPT_HEADER.template
│   │   │   ├── YAML_HEADER.template
│   │   │   ├── CMAKE_HEADER.template
│   │   │   ├── RTL_HEADER.template
│   │   │   ├── DTS_HEADER.template
│   │   │   ├── INTERFACE_HEADER.template
│   │   │   ├── MARKDOWN_HEADER.template
│   │   │   ├── XML_HEADER.template
│   │   │   ├── DOCKERFILE_HEADER.template
│   │   │   └── JSON_GUIDANCE.md
│   │   └── ...
│   │
│   ├── requirements/
│   │   └── ...
│   │
│   ├── design/
│   │   └── ...
│   │
│   └── user/
│       └── ...
│
├── include/
│   └── kritva/
│       └── ...
│
├── src/
│   └── ...
│
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── contract/
│   └── system/
│
├── examples/
│   └── ...
│
├── scripts/
│   ├── build/
│   ├── test/
│   ├── lint/
│   └── ...
│
├── toolchain/
│   ├── VERSIONS.yaml
│   ├── docker/
│   └── scripts/
│
├── configs/
│   └── ...
│
├── third_party/
│   └── ...
│
├── submodules/
│   └── ...
│
├── CMakeLists.txt
├── CMakePresets.json
├── Makefile
├── AGENTS.md
├── LICENSE
├── NOTICE
├── SECURITY.md
├── CONTRIBUTING.md
├── README.md
├── VERSION
├── CHANGELOG.md
├── .gitignore
├── .editorconfig
└── .clang-format
```

> The structure is intentionally modular. Empty directories do not need to be created until content exists.

---

## 3. Repository-Level Files

| File | Responsibility |
|---|---|
| `README.md` | Project introduction, purpose, quick start, architecture overview, build/use entry point |
| `CONTRIBUTING.md` | Contributor workflow, coding practices, PR expectations, testing and contribution process |
| `LICENSE` | Authoritative open-source license for the repository |
| `NOTICE` | Copyright, third-party, trademark, and licensing notices |
| `SECURITY.md` | Security vulnerability reporting and coordinated disclosure process |
| `AGENTS.md` | Instructions and engineering rules for AI coding agents and repository automation |
| `VERSION` | Current repository/project version when a single repository version is required |
| `CHANGELOG.md` | Human-readable release/change history |
| `CMakeLists.txt` | Top-level CMake build definition |
| `CMakePresets.json` | Standardized CMake configure/build/test presets |
| `Makefile` | Simple developer-facing commands that wrap common build/test/toolchain operations |
| `.gitignore` | Files and directories excluded from Git |
| `.editorconfig` | Cross-editor formatting conventions |
| `.clang-format` | C/C++ formatting rules |

---

## 4. `.github/`

### Responsibility

All GitHub-specific repository automation and contribution configuration.

```text
.github/
├── workflows/
├── ISSUE_TEMPLATE/
├── PULL_REQUEST_TEMPLATE.md
└── CODEOWNERS
```

### `workflows/`

CI/CD workflows.

Typical responsibilities:

- configure;
- build;
- unit tests;
- integration tests;
- formatting checks;
- static analysis;
- documentation checks;
- release automation.

### `CODEOWNERS`

Defines ownership/review responsibility for important areas.

This is particularly useful when different KritvaOS teams own Core, Sense, Mind, Motion, SDK, infrastructure, etc.

### `PULL_REQUEST_TEMPLATE.md`

Defines the minimum information expected in pull requests.

---

## 5. `.devcontainer/`

Provides the developer-container definition.

```text
.devcontainer/
├── Dockerfile
└── devcontainer.json
```

### Responsibility

Provides a reproducible development environment for contributors.

The canonical toolchain remains defined by `toolchain/`; the devcontainer should consume that environment rather than independently inventing a second toolchain.

---

## 6. `docs/`

All project documentation that is not repository policy/legal metadata.

Recommended organization:

```text
docs/
├── architecture/
├── development/
├── requirements/
├── design/
└── user/
```

### `docs/architecture/`

System and software architecture.

Examples:

- system architecture;
- component boundaries;
- Nexus/Edge architecture;
- runtime architecture;
- interface architecture;
- deployment architecture.

### `docs/development/`

Engineering process and developer standards.

Examples:

- repository structure;
- development workflow;
- coding standards;
- testing methodology;
- source-header templates.

### `docs/requirements/`

Requirements and traceability material.

### `docs/design/`

Detailed design documents that are below the architectural level.

### `docs/user/`

User/developer-facing usage documentation.

---

## 7. `docs/development/templates/`

### Responsibility

Canonical development templates.

This is the location for the source-header templates previously defined for KritvaOS.

The canonical date format is:

```text
DD-MM-YYYY
```

Example:

```text
Created     : 22-09-2026
```

Component repositories may reference these standards rather than creating conflicting local versions.

---

## 8. `include/`

Public C/C++ API headers owned by the main repository.

Recommended:

```text
include/
└── kritva/
```

Only headers that form a supported public API should normally live here.

Private/internal headers should generally remain under the implementation tree rather than being exposed as public API.

---

## 9. `src/`

Implementation source code owned by the main repository.

Examples:

```text
src/
├── core/
├── runtime/
├── platform/
└── ...
```

The exact subdirectories should follow the architecture rather than becoming arbitrary organizational buckets.

If a component becomes sufficiently independent, it may move to its own repository such as `kritva-core`.

---

## 10. `tests/`

All automated tests owned by the repository.

```text
tests/
├── unit/
├── integration/
├── contract/
└── system/
```

### `unit/`

Tests individual classes, functions, or small modules.

### `integration/`

Tests interactions between multiple modules/components.

### `contract/`

Tests stable interfaces and contracts between components.

### `system/`

End-to-end/system-level tests.

---

## 11. `examples/`

Small, working examples showing how to use KritvaOS APIs or capabilities.

Examples should be:

- buildable;
- understandable;
- minimal;
- maintained with the APIs they demonstrate.

Examples should not become an uncontrolled second implementation of the product.

---

## 12. `scripts/`

Developer and repository automation scripts.

Recommended categories:

```text
scripts/
├── build/
├── test/
├── lint/
└── ...
```

Scripts should automate repeatable repository operations.

General-purpose project source code should not be hidden inside `scripts/`.

---

## 13. `toolchain/`

Canonical shared development toolchain.

```text
toolchain/
├── VERSIONS.yaml
├── docker/
└── scripts/
```

### Responsibility

Defines the versions and construction of the common KritvaOS development environment.

The established development image is:

```text
kritvaos-dev:0.1
```

The principle is:

> Component repositories are independently buildable and independently versioned, while sharing a versioned KritvaOS development toolchain.

Component repositories should consume the common toolchain rather than duplicating independent Docker/toolchain definitions unless there is a documented reason.

---

## 14. `configs/`

Repository-owned configuration files that are part of the product/development system but are not source code.

Examples:

- default runtime configuration;
- development profiles;
- test configurations;
- platform configurations.

Do not place secrets in this directory.

---

## 15. `third_party/`

Third-party source code that is intentionally vendored into the repository.

Each third-party package should preserve its original license and notices.

Recommended:

```text
third_party/
└── <vendor-or-project>/
    ├── LICENSE
    ├── NOTICE
    └── ...
```

Do not copy third-party code into the repository without checking its license compatibility.

---

## 16. `submodules/`

Reserved for independently versioned repositories that are intentionally integrated into the main repository as Git submodules.

Potential future examples:

```text
submodules/
├── kritva-core/
├── kritva-sense/
├── kritva-mind/
├── kritva-motion/
├── kritva-skill/
├── kritva-sim/
└── kritva-sdk/
```

The exact integration strategy may evolve.

A repository should not become a submodule merely because it exists independently. Integration should be justified by architecture and release management needs.

---

## 17. Relationship to Independent Component Repositories

The following repositories are expected to be independently versioned:

```text
kritva-core
kritva-sense
kritva-mind
kritva-motion
kritva-skill
kritva-sim
kritva-sdk
```

The main `KritvaOS` repository acts as the system-level integration point.

A component repository should contain only the documentation, source, tests, build configuration, and tooling required to independently develop and release that component.

Avoid copying the entire main-repository structure into every component repository.

---

## 18. Community Repository

The Community product repository is:

```text
kritvaos-community
```

It is separate from the component repositories and represents the open-source KritvaOS Community distribution/integration layer.

The Community repository remains Apache-2.0.

The Community repository may consume the independently versioned KritvaOS components and shared toolchain.

---

## 19. What Should NOT Go in the Main Repository

Avoid placing the following in the main repository unless explicitly required:

- generated build output;
- temporary files;
- IDE-specific state;
- credentials or secrets;
- customer confidential material;
- large binary artifacts;
- unreviewed third-party code;
- experimental code without an identified owner;
- duplicate copies of component repositories;
- generated documentation that can be reproduced from source.

---

## 20. Ownership Principle

Use this rule when deciding where a file belongs:

```text
Architecture / project-wide policy
        ↓
KritvaOS main repository

Component-specific implementation
        ↓
Independent component repository

Shared development environment
        ↓
KritvaOS/toolchain

Generated output
        ↓
Build/output workspace — not Git

Third-party software
        ↓
third_party/ or an explicit dependency mechanism

Repository automation
        ↓
.github/ and scripts/
```

---

## 21. Canonical Rule

The most important rule is:

> **Every file should have one clear owner, one clear purpose, and one clear location.**

If a file appears to belong to multiple components, first determine whether it is:

1. a shared architectural/interface definition;
2. a true reusable component;
3. a project-level development/tooling artifact; or
4. duplicated functionality that should be eliminated.

This prevents the main KritvaOS repository from becoming a large collection of unrelated code.

---

## 22. Recommended Document Location

Save this document in the main repository as:

```text
docs/development/REPOSITORY_STRUCTURE.md
```

This document should be treated as the canonical repository-organization reference for contributors and interns.
