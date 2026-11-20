# Contributing to KritvaOS Community

Thank you for contributing to **KritvaOS Community**.

KritvaOS Community is intended to be the permanent open-source foundation of the KritvaOS platform. Contributions should preserve the openness, architectural integrity, quality, and long-term maintainability of the project.

## 1. Community License

KritvaOS Community source code is licensed under the **Apache License, Version 2.0**.

The Community license is Apache-2.0 (owner decision D-064).

See `LICENSE`.

The Apache-2.0 license applies unless a file or component explicitly identifies another applicable license.

## 2. Contribution and Enterprise Use

KritvaOS Enterprise may use accepted KritvaOS Community contributions together with separately licensed Enterprise components.

```text
Contributor
     │
     │ contribution
     ▼
KritvaOS Community
     │
     ├──────────────► Community releases
     │
     └──────────────► KritvaOS Enterprise
```

Community code remains governed by its applicable Community license. Inclusion in an Enterprise product does not convert Community code into proprietary software.

## 3. Contributor Rights

Contributors generally retain copyright ownership of their original contributions unless a separate written agreement provides otherwise.

By submitting a contribution, the contributor grants the project the rights necessary to review, maintain, reproduce, modify, integrate, test, distribute, and otherwise use the contribution under the applicable project and contribution terms.

No Contributor License Agreement is required today. Contributions are accepted under the Developer Certificate of Origin (section 4). A CLA is deferred; see [CLA.md](CLA.md).

## 4. Developer Certificate of Origin (DCO)

KritvaOS Community accepts contributions under the Developer Certificate of Origin, version 1.1 (<https://developercertificate.org>). No Contributor License Agreement is required today.

By adding a `Signed-off-by:` line to a commit, the contributor certifies the DCO for that commit. In short, the contributor confirms that they wrote the contribution or otherwise have the right to submit it under the project license (Apache-2.0), and that they understand the contribution and the sign-off are public and kept with the project.

- Sign off each commit with `git commit -s`, which appends `Signed-off-by: Your Name <you@example.com>` using your Git name and email. Use your real name.
- Contributors keep the copyright in their contributions. Contributions are licensed to the project and its users under Apache-2.0, the same licence as the rest of KritvaOS Community. No copyright assignment is requested.
- Contributions made for an employer or organisation need that employer's or organisation's authorisation before they are signed off.
- The `Signed-off-by:` line goes beside the `Task:` footer described in section 20 and does not replace it.

Enforcement: the `commit-message-check` workflow runs `scripts/security/check_dco_signoff.py` on every pull request. Each new commit needs a `Signed-off-by: Name <email>` line whose email matches the commit author or committer, and commits by GitHub bot accounts are exempt. Existing history is not checked. To fix a missing sign-off, run `git commit --amend -s` for the last commit, or `git rebase --signoff origin/main` for several, then force-push your branch. You can check locally with `python3 scripts/security/check_dco_signoff.py --range origin/main..HEAD`.

A CLA is deferred, not rejected. [CLA.md](CLA.md) is kept as an unadopted draft. Adopting a CLA would be a separate owner decision, after legal review, and would be recorded in kritva-program. It would not change the permanent Apache-2.0 status of KritvaOS Community.

## 5. Who Can Contribute?

Contributions may come from individual developers, KritvaOS employees, interns, contractors, students, universities, research organizations, robotics companies, semiconductor companies, and other organizations.

Contributors must have authority to submit the material they contribute.

## 6. Intern Contributions

Intern contributions are subject to the same technical and contribution requirements as other contributions.

Before submission:

1. define the work package;
2. understand ownership/IP requirements;
3. check applicable internship or employment agreements;
4. complete applicable contributor requirements;
5. pass normal review.

## 7. Corporate Contributions

If acting on behalf of an organization, the contributor must have appropriate authorization.

Do not submit employer-owned, customer-owned, confidential, or proprietary third-party code without authorization.

## 8. Third-Party Code

Do not copy third-party code into KritvaOS Community without checking its license.

Document, where applicable:

```text
Project:
Source:
Copyright:
License:
URL:
Modification:
```

Preserve original notices where required.

## 9. License Compatibility

Do not intentionally introduce material with licensing terms incompatible with the intended Community distribution.

Particular care is required for proprietary code, code without a license, source-available software, restrictive licenses, copied snippets of uncertain origin, and generated code with unclear licensing status.

When uncertain, request a licensing review before submission.

## 10. Intellectual Property and Patents

Contributors must not knowingly submit technology they do not have the right to contribute.

Known patent or intellectual-property concerns relevant to a contribution should be disclosed.

The final treatment of copyright and patent rights is governed by the applicable project license and the Developer Certificate of Origin sign-off described in section 4.

## 11. What Makes a Good Contribution?

A good contribution should be:

- technically correct;
- architecturally appropriate;
- testable;
- documented;
- maintainable;
- reproducible;
- focused on a clearly defined problem.

Where applicable:

```text
Requirement
    ↓
API
    ↓
Implementation
    ↓
Test
```

## 12. Before Starting Development

For significant changes:

1. check existing issues and architecture documentation;
2. identify the affected component;
3. identify applicable requirements;
4. discuss architectural changes before implementation;
5. define the intended API;
6. define verification expectations.

Avoid implementing large architectural changes solely inside a pull request without prior design discussion.

## 13. Coding Standards

Follow the standards of the affected repository.

For C++:

- C++20 unless otherwise specified;
- repository `.clang-format`;
- clear ownership and lifetime semantics;
- explicit interfaces;
- warnings treated seriously;
- no unnecessary platform dependencies.

Documentation must preserve terminology defined by the architecture.

## 14. Kritva Core Rules

Contributions to `kritva-core` must preserve Core's platform-independent role.

Core should not directly depend on:

```text
ROS2
DDS
EtherCAT
specific robot vendors
specific MCU SDKs
cloud platforms
commercial databases
robot-specific algorithms
```

unless an approved architecture decision explicitly changes the boundary.

## 15. Testing Requirements

Contributions should include appropriate tests.

Depending on the change:

```text
Unit Test
Contract Test
Integration Test
Simulation Test
Hardware-in-the-Loop Test
```

For Core Foundation changes, unit tests and API/contract tests are normally expected.

## 16. Documentation Requirements

Public API changes should update appropriate documentation, such as:

```text
API.md
REQUIREMENTS.md
ARCHITECTURE.md
TESTING.md
CHANGELOG.md
```

New public APIs should document purpose, ownership, lifetime, inputs, outputs, failure behavior, thread-safety assumptions where relevant, and compatibility expectations.

## 17. File Header Standard

KritvaOS Community source files should use the standard header:

```cpp
//==============================================================================
// Copyright (c) 2026 Kritva Robotics Private Limited.
// SPDX-License-Identifier: Apache-2.0
//
// File        : example.hpp
// Description : Example KritvaOS API.
//
// Component   : Kritva Core
// Module      : Example
// Layer       : Core Foundation
//
// Requirements: CORE-XXX-001
// API         : CORE-API-XXX
//
// Author      : KritvaOS Core Team
// Created     : YYYY-MM-DD
//==============================================================================
```

The legal copyright holder may be updated in the future when the legal ownership structure is finalized.

## 18. Pull Request Process

```text
Issue / Requirement
        ↓
Design / Architecture Review
        ↓
Implementation
        ↓
Local Tests
        ↓
Pull Request
        ↓
CI
        ↓
Code Review
        ↓
Architecture Review (if required)
        ↓
Approval
        ↓
Merge
```

## 19. Pull Request Description

A meaningful pull request should describe:

- **Problem** — what problem is solved?
- **Solution** — what changed?
- **Scope** — which components are affected?
- **Requirements** — which requirement IDs are addressed?
- **Verification** — what tests were run?
- **Compatibility** — does public API or behavior change?
- **Licensing** — does it contain third-party material?

## 20. Commit Guidelines

Commit messages follow the Kritva commit standard (kritva-program decision D-053, adopted here by D-061 and D-062). The controlling text is [AGENTS.md](AGENTS.md), section 48.

- Header: `type(scope): summary`, at most 90 characters. Types: feat, fix, docs, refactor, test, build, ci, chore, perf, revert, security.
- One footer line: `Task: <ID>` or `Tasks: <ID>, <ID>`. A docs or chore commit that implements no task uses `Tasks: None (reason)`.
- Sign off every commit with `git commit -s`, which adds a `Signed-off-by:` line (section 4). The commit-format check accepts it beside the `Task:` footer.
- Do not add a `Claude-Session:` trailer or a claude.ai session link. A `Co-Authored-By:` line is allowed.
- Check your commits with `python3 scripts/security/check_commit_format.py --range origin/main..HEAD`; the `commit-message-check` workflow runs the same check.

Examples:

```text
feat(core): add lifecycle state API

Task: <task ID from the planning records>
```

```text
docs(readme): correct the build command

Tasks: None (record correction)
```

Avoid messages such as:

```text
fix
changes
update
test
misc
```


## 21. Review Responsibilities

### Code Review

Correctness, maintainability, style, tests, and implementation quality.

### Architecture Review

System boundaries, API design, dependency direction, compatibility, and architectural consistency.

### Verification Review

Requirement traceability, test adequacy, regression coverage, and evidence.

### Security Review

Security-sensitive changes, trust boundaries, input validation, credentials/secrets, and vulnerability implications.

## 22. Security Vulnerabilities

Do not report security vulnerabilities through public GitHub issues.

Use the process described in `SECURITY.md`.

## 23. No Guarantee of Acceptance

A contribution is not guaranteed to be accepted.

Maintainers may accept, request changes, defer, reject, redesign, replace, or partially accept a contribution based on technical, architectural, security, licensing, or project requirements.

## 24. Community Governance

Significant architectural decisions should be documented through Architecture Decision Records where appropriate.

Recommended location:

```text
docs/adr/
```

## 25. Enterprise Boundary

KritvaOS Community should remain a useful and functional open-source platform.

KritvaOS Enterprise may provide additional capabilities such as:

- commercial support;
- enterprise deployment;
- fleet management;
- advanced security;
- enterprise observability;
- certified releases;
- proprietary integrations;
- commercial hardware;
- proprietary IP;
- customer-specific services.

## 26. License Compliance

Contributors and maintainers must preserve applicable license obligations.

For Apache-2.0 code, this includes applicable copyright notices, license notices, NOTICE requirements, attribution requirements, and patent provisions.

Enterprise distributions containing Community components must maintain applicable Apache-2.0 compliance information.

## 27. Contributor Recognition

KritvaOS may recognize contributors through Git history, release notes, contributor lists, documentation, and project acknowledgements.

Recognition does not by itself determine copyright ownership.

## 28. Questions About Contribution Rights

If you are uncertain whether you can contribute something, do not guess.

Examples:

```text
Can I contribute this code?
Is this library license compatible?
Does my employer own this code?
Can I submit this university project?
Can this third-party snippet be used?
```

Raise the question before submitting the material.

For legal questions, obtain appropriate professional legal advice.

## 29. Core Community Principle

> **KritvaOS Community is the permanent open-source foundation of KritvaOS.**

The objective is to build a reusable, open, hardware-aware robotic computing platform that can be used by the broader robotics community while providing a stable foundation for KritvaOS Enterprise.

## 30. Final Contribution Principle

```text
Open contribution
       ↓
Strong architecture
       ↓
Verified implementation
       ↓
Apache-2.0 Community
       ↓
Reusable KritvaOS foundation
       ↓
KritvaOS Enterprise
```

Thank you for contributing to KritvaOS Community.
