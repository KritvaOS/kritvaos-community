# KritvaOS Community

## Open Robotic Computing Platform

KritvaOS Community is the open-source foundation of the **KritvaOS Open Robotic Computing Platform**.

KritvaOS provides a common architecture connecting robotic intelligence, real-time software, distributed compute, hardware interfaces, simulation, and physical robotic systems.

**KritvaOS Community remains Apache License 2.0 licensed permanently.**

## Vision

> **KritvaOS connects Physical AI to the physical world.**

A robot is a distributed computing system that senses, reasons, controls, communicates, and interacts with the physical world in real time.

```text
Application
     ↓
Skill
     ↓
Mind
     ↓
Sense / Motion
     ↓
Kritva Core
     ↓
Hardware Abstraction
     ↓
Nexus / Edge
     ↓
Subnode
     ↓
Endpoint
     ↓
Physical World
```

## KritvaOS Community

KritvaOS Community is the open foundation of the KritvaOS ecosystem.

It provides:

- open core APIs and runtime foundations;
- robotic software components;
- hardware abstraction interfaces;
- simulation and validation infrastructure;
- developer SDKs and tools;
- reference integrations;
- documentation and examples;
- an open contribution model.

Community components are licensed under **Apache License 2.0**.

### Permanent Community License

KritvaOS Community is intended to remain Apache-2.0 licensed permanently. The Community codebase is not intended to be converted into a proprietary or source-available license as part of the normal evolution of the project.

## KritvaOS Enterprise

KritvaOS Enterprise is the commercial layer built around the KritvaOS platform.

Enterprise may combine KritvaOS Community components with separately licensed commercial components and services.

Potential Enterprise capabilities include:

- enterprise deployment;
- fleet management;
- advanced device management;
- enterprise security;
- remote diagnostics;
- advanced observability;
- commercial support and SLAs;
- certified releases;
- proprietary integrations;
- customer-specific engineering;
- commercial hardware and IP.

The inclusion of Community software in an Enterprise product does not change the license of the underlying Community software.

```text
                    KritvaOS
                       │
        ┌──────────────┴──────────────┐
        │                             │
KritvaOS Community              KritvaOS Enterprise
    Apache-2.0                  Commercial Layer
        │                             │
        └──────────────┬──────────────┘
                       │
                Common Architecture
```

## Community Repositories

The KritvaOS Community ecosystem may contain independent repositories:

```text
kritvaos-community
kritva-core
kritva-sense
kritva-mind
kritva-motion
kritva-skill
kritva-sim
kritva-sdk
```

`kritvaos-community` provides Community-level documentation, governance, contribution policy, and ecosystem baselines.

## Kritva Core

`kritva-core` provides the platform-independent Core Foundation API:

- identity;
- versioning;
- timestamp and duration;
- metadata;
- lifecycle;
- status;
- health;
- statistics;
- errors and `Result<T>`;
- events;
- capabilities;
- configuration.

Core intentionally avoids direct dependencies on ROS2, DDS, EtherCAT, specific robot vendors, specific MCU SDKs, cloud services, commercial databases, and robot-specific algorithms unless explicitly approved by architecture.

## Architecture

```text
Applications
      ↓
Kritva SDK
      ↓
Kritva Skill
      ↓
┌───────────────┬───────────────┐
│               │               │
Mind          Motion          Sense
│               │               │
└───────────────┴───────────────┘
              ↓
         Kritva Core
              ↓
    Hardware Abstraction
              ↓
       Nexus / Edge
              ↓
          Subnode
              ↓
         Endpoint
              ↓
      Physical World
```

KritvaOS is intended to support multiple robotic systems, including humanoids, manipulators/cobots, mobile robots, quadrupeds, aerial systems, and inspection/service robots.

## Development Philosophy

```text
Requirements
    ↓
Architecture
    ↓
API
    ↓
Implementation
    ↓
Unit Tests
    ↓
Contract Tests
    ↓
Integration Tests
    ↓
Verification
    ↓
Release
```

Contributions are expected to preserve architectural boundaries and include appropriate verification evidence.

## Contribution

KritvaOS Community welcomes contributions from individual developers, robotics engineers, researchers, universities, students and interns, open-source developers, companies, and organizations.

Before contributing, read:

- `CONTRIBUTING.md`
- `CLA.md` where applicable
- `LICENSE`
- `NOTICE`
- `SECURITY.md`

Contributors must have the necessary rights and authorization to submit their contributions.

Accepted contributions may be used in KritvaOS Community and in products built from the Community platform, including KritvaOS Enterprise, subject to applicable licensing and contribution terms.

## Licensing

KritvaOS Community is licensed under the **Apache License, Version 2.0**.

See `LICENSE`.

SPDX identifier:

```text
Apache-2.0
```

Source files should use the standard KritvaOS header and SPDX identifier.

## Third-Party Software

Third-party software must retain applicable copyright and license information.

Contributors must identify third-party code and verify that its licensing terms permit the intended use.

## Security

Security vulnerabilities should not be disclosed through public GitHub issues. Follow the process documented in `SECURITY.md`.

## Development Environment

KritvaOS uses a versioned development toolchain. The canonical development environment may be provided through a versioned container such as:

```text
kritvaos-dev:0.1
```

The objective is:

```text
Developer Environment
        =
CI Environment
```

Component repositories should consume the common KritvaOS toolchain rather than independently defining incompatible development environments.

## Repository Status

KritvaOS Community is an early-stage/pre-alpha open-source project.

APIs, repository boundaries, hardware interfaces, and implementation details may evolve as the architecture is validated. Early APIs should not be assumed to be stable production interfaces unless explicitly marked as such.

## Community Principle

> **KritvaOS Community is the permanent open-source foundation of KritvaOS.**

The goal is to build a reusable, open, hardware-aware robotic computing platform that connects intelligence, software, distributed compute, hardware, and physical action.
