# Contributing to KritvaOS Community

Thank you for contributing to **KritvaOS Community**.

KritvaOS Community is intended to be the permanent open-source foundation of the KritvaOS platform. Contributions should preserve the openness, architectural integrity, quality, and long-term maintainability of the project.

## 1. Community License

KritvaOS Community source code is licensed under the **Apache License, Version 2.0**.

The Community license is intended to remain Apache-2.0 permanently.

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

A formal Contributor License Agreement may be required.

## 4. Contributor License Agreement

KritvaOS may require contributors to complete a CLA.

The CLA is intended to establish that:

- the contributor has the right to submit the contribution;
- required employer or organizational authorization has been obtained;
- the contribution may be used by the KritvaOS project;
- the contribution may be distributed under the Community license;
- applicable copyright rights are granted;
- applicable patent rights are addressed.

The CLA is not intended to change the permanent Apache-2.0 status of KritvaOS Community.

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

The final treatment of copyright and patent rights is governed by the applicable project license and, where required, the KritvaOS CLA.

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
// Copyright (c) 2026 KritvaOS
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

Use concise, descriptive commit messages.

Examples:

```text
Add Core lifecycle state API
Add Core Result error propagation
Add configuration parameter model
Fix timestamp comparison semantics
Add capability contract tests
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
