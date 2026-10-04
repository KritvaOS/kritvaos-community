# Kritva Repository Architecture

## 1. Purpose

This document defines how the Kritva source tree is organized to implement the system architecture defined in the root [`ARCHITECTURE.md`](../../ARCHITECTURE.md).

It answers:

- Where each Kritva component lives
- Which components are tightly coupled
- Where repository boundaries may exist
- How dependencies should flow
- How Git submodules may be used
- How hardware, software, simulation, and verification are organized
- How the repository can evolve without premature fragmentation

The repository architecture is an implementation structure, not a replacement for the system architecture.

---

## 2. Relationship to System Architecture

The architectural hierarchy is:

```text
ARCHITECTURE.md
      ↓
System / Component Boundaries
      ↓
REPOSITORY_ARCHITECTURE.md
      ↓
Source Tree
      ↓
Build / Test / Verification
```

The root `ARCHITECTURE.md` defines **what the system is**.

This document defines **where the implementation lives**.

---

## 3. Repository Philosophy

Kritva should initially optimize for:

1. Architectural clarity
2. Fast cross-layer development
3. Easy simulation
4. Easy verification
5. Low integration overhead
6. Reusable interfaces
7. Clear ownership
8. Controlled dependency direction

The project should avoid splitting repositories merely because directories represent different components.

Repository boundaries should be introduced when there is a concrete engineering or organizational reason.

---

## 4. Initial Repository Model

The recommended initial model is a **modular monorepo**.

```text
KritvaOS/
│
├── core/
├── sense/
├── mind/
├── motion/
├── skill/
├── sim/
├── sdk/
│
├── hardware/
├── drivers/
│
├── soc/
│   ├── nexus/
│   └── edge/
│
├── examples/
├── tests/
├── tools/
├── docs/
│
├── README.md
├── ARCHITECTURE.md
├── CONTRIBUTING.md
├── LICENSE
└── VERSION
```

This model is appropriate during early architecture and pre-alpha development because changes often cross multiple layers.

---

## 5. Why a Monorepo Initially

A monorepo provides several advantages during architecture validation.

### 5.1 Cross-Layer Changes

A change to Core may affect:

```text
Core
 ↓
Sense
 ↓
Motion
 ↓
Skill
 ↓
SDK
```

Keeping these components together makes architectural iteration easier.

### 5.2 Unified Testing

The same repository can execute:

- Unit tests
- Contract tests
- Integration tests
- Simulation tests
- Renode tests
- Hardware-in-the-loop tests

### 5.3 Shared Interfaces

Common interfaces can be evolved without coordinating multiple external repositories.

### 5.4 Early Architecture Validation

The project is still determining:

- API boundaries
- Module boundaries
- Hardware/software boundaries
- Repository boundaries

Premature repository separation would make these changes more expensive.

---

## 6. Top-Level Directory Structure

Recommended structure:

```text
KritvaOS/
│
├── core/
├── sense/
├── mind/
├── motion/
├── skill/
├── sim/
├── sdk/
│
├── hardware/
├── drivers/
│
├── soc/
│   ├── nexus/
│   └── edge/
│
├── examples/
├── tests/
├── tools/
│
├── docs/
│   ├── architecture/
│   ├── requirements/
│   ├── api/
│   ├── verification/
│   └── adr/
│
├── scripts/
│
├── README.md
├── ARCHITECTURE.md
├── CONTRIBUTING.md
├── LICENSE
├── VERSION
├── AGENTS.md
└── .gitignore
```

Not every directory must be implemented immediately.

The structure establishes the intended architectural destination.

---

## 7. Software Components

The main KritvaOS software modules are:

```text
core
sense
mind
motion
skill
sim
sdk
```

Their responsibilities should remain aligned with the system architecture.

---

## 8. `core/`

`core/` contains the platform-independent Kritva Core Foundation.

Recommended structure:

```text
core/
├── include/
│   └── kritva/
│       └── core/
│           ├── types/
│           ├── lifecycle/
│           ├── status/
│           ├── health/
│           ├── statistics/
│           ├── error/
│           ├── event/
│           ├── capability/
│           ├── configuration/
│           └── core.hpp
│
├── src/
├── tests/
│   ├── unit/
│   └── contract/
│
├── examples/
├── docs/
├── scripts/
│
├── CMakeLists.txt
├── CMakePresets.json
├── AGENTS.md
├── README.md
├── ARCHITECTURE.md
├── REQUIREMENTS.md
├── API.md
├── TESTING.md
├── CONTRIBUTING.md
├── INTERNSHIP.md
├── CODEOWNERS
├── CHANGELOG.md
├── VERSION
└── .clang-format
```

Core should remain small and stable.

---

## 9. Core Dependency Boundary

Preferred dependency direction:

```text
Application
    ↓
SDK
    ↓
Skill
    ↓
Mind / Motion
    ↓
Sense
    ↓
Core
```

Core should not depend on higher-level robotics modules.

In particular:

```text
Core
  X→ Mind
  X→ Motion
  X→ Skill
  X→ Sense
  X→ Application
```

This prevents architectural inversion.

---

## 10. `sense/`

`sense/` contains perception and sensor-related software.

Potential structure:

```text
sense/
├── include/
├── src/
├── tests/
│   ├── unit/
│   ├── contract/
│   └── integration/
├── examples/
├── docs/
└── CMakeLists.txt
```

Potential subcomponents may include:

```text
sense/
├── sensors/
├── synchronization/
├── fusion/
├── perception/
├── tracking/
└── estimation/
```

Subdirectories should be introduced only when their interfaces are understood.

---

## 11. `mind/`

`mind/` contains intelligence and reasoning components.

Potential structure:

```text
mind/
├── include/
├── src/
├── models/
├── runtimes/
├── planning/
├── reasoning/
├── world_model/
├── tests/
├── examples/
└── docs/
```

AI model artifacts should not automatically become source-code dependencies.

Model storage and deployment should remain configurable.

---

## 12. `motion/`

`motion/` contains motion and control software.

Potential structure:

```text
motion/
├── include/
├── src/
├── kinematics/
├── dynamics/
├── planning/
├── locomotion/
├── manipulation/
├── control/
├── safety/
├── tests/
└── docs/
```

Low-level hardware drivers should not be placed here merely because they control motors.

Motion should consume hardware abstractions.

---

## 13. `skill/`

`skill/` contains reusable robotic capabilities.

Potential structure:

```text
skill/
├── include/
├── src/
├── skills/
├── composition/
├── lifecycle/
├── tests/
├── examples/
└── docs/
```

Skills may depend on:

```text
Skill
 ├── Mind
 ├── Motion
 └── Sense
```

Skills should not directly depend on hardware-specific drivers.

---

## 14. `sim/`

`sim/` contains simulation and validation integration.

Potential structure:

```text
sim/
├── environments/
├── robots/
├── sensors/
├── actuators/
├── scenarios/
├── digital_twin/
├── sil/
├── hil/
├── regression/
├── tests/
└── docs/
```

Simulation should reuse the same APIs used by physical implementations wherever practical.

---

## 15. `sdk/`

`sdk/` contains developer-facing interfaces.

Potential structure:

```text
sdk/
├── include/
├── src/
├── examples/
├── tools/
├── bindings/
├── tests/
└── docs/
```

The SDK should expose supported customer-facing APIs without exposing unnecessary internal implementation details.

---

## 16. Hardware Directory

`hardware/` contains hardware abstraction and platform integration that does not belong exclusively to one SoC implementation.

Possible structure:

```text
hardware/
├── abstraction/
├── sensors/
├── actuators/
├── interfaces/
├── boards/
├── platforms/
└── docs/
```

Hardware-specific drivers may live under `drivers/`.

---

## 17. Drivers Directory

`drivers/` contains hardware-specific software interfaces.

Potential structure:

```text
drivers/
├── sensors/
├── actuators/
├── communication/
├── storage/
├── gpio/
├── spi/
├── i2c/
├── uart/
├── pwm/
├── adc/
├── ethercat/
└── docs/
```

Drivers should depend downward toward hardware and upward only through defined abstractions.

---

## 18. SoC Directory

The `soc/` directory contains hardware/software co-design assets for Kritva compute platforms.

```text
soc/
├── nexus/
└── edge/
```

Each platform is treated as a family rather than one fixed chip.

---

## 19. Nexus Repository Structure

Recommended initial structure:

```text
soc/nexus/
├── rtl/
├── sw/
├── fpga/
├── renode/
├── sim/
├── verification/
├── docs/
├── tests/
├── scripts/
└── README.md
```

Potential future separation:

```text
kritva-nexus-hw
kritva-nexus-sw
kritva-nexus-verification
```

should happen only when justified.

---

## 20. Edge Repository Structure

Recommended initial structure:

```text
soc/edge/
├── rtl/
├── sw/
├── fpga/
├── renode/
├── sim/
├── verification/
├── docs/
├── tests/
├── scripts/
└── README.md
```

Edge software may include:

- Firmware
- Drivers
- Real-time runtime
- Communication stack
- Local diagnostics

---

## 21. Nexus / Edge Boundary

The repository should preserve the conceptual boundary:

```text
Nexus
├── System-level compute
├── AI
├── Vision
├── Planning
└── Global services

Edge
├── Real-time compute
├── Motion control
├── I/O
├── EtherCAT
└── Local diagnostics
```

However, the physical implementation may vary by product.

---

## 22. Subnode and Endpoint

Subnode and Endpoint implementations may be distributed across:

```text
hardware/
drivers/
soc/edge/
```

depending on their ownership and implementation.

The architectural model is:

```text
Nexus
  ↓
Edge
  ↓
Subnode
  ↓
Endpoint
```

The repository does not need one directory for every architectural abstraction.

---

## 23. Tests Directory

The top-level `tests/` directory provides cross-component verification.

Recommended structure:

```text
tests/
├── unit/
├── contract/
├── integration/
├── system/
├── simulation/
├── renode/
├── fpga/
├── hil/
└── regression/
```

Component-specific unit tests should remain near their implementation.

Cross-component tests belong in the top-level verification structure.

---

## 24. Test Ownership

A useful rule is:

```text
Component behavior
    → Component tests

Interface behavior
    → Contract tests

Cross-component behavior
    → Integration tests

System behavior
    → System tests

Hardware behavior
    → FPGA / HIL tests
```

This prevents the top-level test directory from becoming a second source tree.

---

## 25. Examples

`examples/` contains small, runnable demonstrations.

Potential examples:

```text
examples/
├── core/
├── sense/
├── motion/
├── skill/
├── mind/
├── sdk/
├── simulation/
├── nexus/
└── edge/
```

Examples should demonstrate supported public interfaces.

---

## 26. Tools

`tools/` contains developer and validation utilities.

Examples:

```text
tools/
├── cli/
├── codegen/
├── analysis/
├── testing/
├── simulation/
├── documentation/
└── packaging/
```

Tools should not become a dumping ground for application logic.

---

## 27. Scripts

`scripts/` contains repository-level automation.

Examples:

```text
scripts/
├── build/
├── test/
├── lint/
├── format/
├── ci/
├── release/
└── documentation/
```

Scripts should be small, reproducible, and documented.

---

## 28. Documentation Structure

Recommended documentation layout:

```text
docs/
├── architecture/
├── requirements/
├── api/
├── verification/
└── adr/
```

---

## 29. Architecture Documentation

```text
docs/architecture/
├── REPOSITORY_ARCHITECTURE.md
├── SOC_ARCHITECTURE.md
├── FPGA_ARCHITECTURE.md
├── RENODE_ARCHITECTURE.md
├── DISTRIBUTED_COMPUTING.md
├── REALTIME_ARCHITECTURE.md
├── SAFETY_ARCHITECTURE.md
└── SECURITY_ARCHITECTURE.md
```

The root `ARCHITECTURE.md` remains the canonical system-level architecture.

---

## 30. Requirements Documentation

```text
docs/requirements/
├── SYSTEM_REQUIREMENTS.md
├── CORE_REQUIREMENTS.md
├── HARDWARE_REQUIREMENTS.md
├── REALTIME_REQUIREMENTS.md
├── SAFETY_REQUIREMENTS.md
└── SECURITY_REQUIREMENTS.md
```

Requirements should be traceable to implementation and verification.

---

## 31. API Documentation

```text
docs/api/
├── CORE_API.md
├── SENSE_API.md
├── MIND_API.md
├── MOTION_API.md
├── SKILL_API.md
├── SDK_API.md
└── HARDWARE_API.md
```

Stable public APIs should be distinguished from internal APIs.

---

## 32. Verification Documentation

```text
docs/verification/
├── VERIFICATION_PLAN.md
├── TEST_STRATEGY.md
├── TRACEABILITY.md
├── SIMULATION.md
├── RENODE.md
├── FPGA.md
└── HIL.md
```

Verification should map back to requirements.

---

## 33. ADR Documentation

Architecture decisions belong under:

```text
docs/adr/
```

Example:

```text
docs/adr/
├── ADR-0001-architecture-principles.md
├── ADR-0002-core-boundary.md
├── ADR-0003-nexus-edge-model.md
├── ADR-0004-repository-model.md
├── ADR-0005-ros2-integration.md
└── ADR-0006-real-time-model.md
```

---

## 34. Dependency Direction

The preferred dependency direction is:

```text
Application
    ↓
SDK
    ↓
Skill
    ↓
Mind / Motion
    ↓
Sense
    ↓
Core
    ↓
Hardware Abstraction
    ↓
Drivers / Platform
    ↓
Hardware
```

Hardware implementations must not force dependencies upward into application-level modules.

---

## 35. Dependency Rules

### Rule 1 — Core is foundational

Core must not depend on higher-level robotic modules.

### Rule 2 — Higher layers consume lower-layer contracts

Higher layers should use interfaces rather than implementation details.

### Rule 3 — Hardware dependencies stay below abstraction

Robot-specific hardware code should not leak into Mind or Skill.

### Rule 4 — Simulation follows interfaces

Simulation should implement or emulate the same contracts where practical.

### Rule 5 — Tests may observe multiple layers

Verification code may cross boundaries when validating system behavior.

---

## 36. Public vs Internal APIs

Each component should distinguish:

```text
Public API
Internal API
Experimental API
```

Suggested convention:

```text
include/kritva/
    → supported public interfaces

src/
    → implementation

internal/
    → implementation-specific interfaces
```

Experimental APIs should be explicitly marked.

---

## 37. Include / Dependency Hygiene

Public headers should avoid unnecessary dependencies.

Prefer:

```text
Public API
    ↓
Stable Core Types
    ↓
Minimal Dependencies
```

Avoid exposing:

- Hardware-specific headers
- Vendor-specific types
- ROS2 types
- DDS types
- Internal implementation classes

unless they are intentionally part of the public API.

---

## 38. Build Architecture

CMake is the recommended baseline build system for C++ components.

Potential top-level build:

```text
CMakeLists.txt
CMakePresets.json
```

Each major component should be independently buildable where practical.

Example:

```text
cmake
  ↓
core
sense
motion
mind
skill
sdk
```

---

## 39. Component Build Independence

A component should ideally support:

```text
Build alone
Build with dependencies
Build as part of complete system
```

For example:

```text
core
   → standalone library

core + sense
   → perception development

core + sense + motion
   → robotics integration

full tree
   → system build
```

This is especially useful for internships and parallel development.

---

## 40. Continuous Integration

CI should progressively validate:

```text
Formatting
   ↓
Static Analysis
   ↓
Build
   ↓
Unit Tests
   ↓
Contract Tests
   ↓
Integration Tests
   ↓
Simulation
   ↓
Packaging
```

Hardware CI can be added separately.

---

## 41. Static Analysis

The repository should establish consistent tooling for:

- Compiler warnings
- Clang-format
- Clang-tidy
- Sanitizers
- Static analyzers
- Dependency checks
- License checks
- Security scanning

Tool selection may evolve.

---

## 42. Versioning

The project should maintain explicit versioning.

Recommended files:

```text
VERSION
CHANGELOG.md
```

Public APIs should follow documented compatibility rules.

Early pre-alpha releases may intentionally allow breaking changes.

---

## 43. Release Structure

A release should define:

- Source revision
- API revision
- Hardware compatibility
- Simulation compatibility
- Verification status
- Known limitations

Example:

```text
Kritva v0.1
│
├── KritvaOS
├── Nexus platform revision
├── Edge platform revision
├── Simulation revision
└── Verification status
```

---

## 44. Hardware / Software Version Matrix

As the platform matures, compatibility should be explicit.

Example:

| KritvaOS | Nexus | Edge | Simulation |
|---|---|---|---|
| v0.1 | N0.1 | E0.1 | S0.1 |
| v0.2 | N0.2 | E0.1 | S0.2 |

The exact scheme can evolve.

---

## 45. Git Submodules

Git submodules may be used for components that have:

- Independent ownership
- Independent release cadence
- Significant reuse
- Large repository size
- Separate access requirements
- Stable interfaces
- External collaboration

They should not be used merely to make the directory tree appear modular.

---

## 46. Potential Submodule Candidates

Future candidates may include:

```text
soc/nexus
soc/edge
sim
```

or independent hardware/IP repositories.

However, no component should be split before its boundary is sufficiently stable.

---

## 47. Hybrid Repository Model

The long-term model may be:

```text
KritvaOS
│
├── Core
├── Sense
├── Mind
├── Motion
├── Skill
├── SDK
│
├── Submodule → Kritva Sim
├── Submodule → Nexus
└── Submodule → Edge
```

The top-level repository remains the integration point.

---

## 48. Repository Split Criteria

A component should be considered for repository separation when several of these conditions are true:

1. API boundary is stable.
2. Ownership is distinct.
3. Release cadence is distinct.
4. Component is reused externally.
5. Independent CI is beneficial.
6. Repository size creates meaningful overhead.
7. Access control requires separation.
8. IP/licensing requirements require separation.

If these conditions are not present, keeping the component in the monorepo is usually simpler.

---

## 49. Repository Ownership

Each major component should eventually have an explicit owner.

Example:

```text
Core          → Core Lead
Sense         → Perception Lead
Mind          → AI Lead
Motion        → Robotics Control Lead
Skill         → Robotics Platform Lead
Sim           → Simulation Lead
Nexus         → Compute Hardware Lead
Edge          → Real-Time Hardware Lead
SDK           → Developer Platform Lead
Verification   → Verification Lead
```

Ownership can be represented using `CODEOWNERS`.

---

## 50. CODEOWNERS

A future `CODEOWNERS` file may define:

```text
/core/          @kritva/core
/sense/         @kritva/sense
/mind/          @kritva/mind
/motion/        @kritva/motion
/skill/         @kritva/skill
/sim/           @kritva/sim
/sdk/           @kritva/sdk
/soc/nexus/     @kritva/nexus
/soc/edge/      @kritva/edge
/tests/         @kritva/verification
```

Actual organization names should follow the project's GitHub organization.

---

## 51. Contribution Model

Contributors should work within defined component boundaries.

A change should normally include:

```text
Requirement
   ↓
Design / API
   ↓
Implementation
   ↓
Tests
   ↓
Documentation
```

Cross-component changes should identify affected owners.

---

## 52. Intern Work Packages

The repository structure should support bounded internship projects.

Example:

```text
Intern 1
   → core/

Intern 2
   → hardware/abstraction/

Intern 3
   → drivers/sensors/

Intern 4
   → soc/edge/

Intern 5
   → sim/

Intern 6
   → tests/verification/
```

Each work package should have a clear:

- Scope
- API
- Requirements
- Tests
- Acceptance criteria

---

## 53. AI-Assisted Engineering

AI tools may assist contributors with:

- Implementation
- Test generation
- Documentation
- Static analysis
- Refactoring
- Review
- Traceability

However, generated code should pass the same:

```text
Build
Test
Static Analysis
Review
Verification
```

requirements as manually written code.

---

## 54. Generated Artifacts

Generated files should be separated from source where practical.

Avoid committing:

```text
build/
coverage/
generated temporary logs
local cache
IDE metadata
```

Generated source that is required for reproducible builds should have explicit ownership and generation instructions.

---

## 55. Configuration Files

Repository-level configuration may include:

```text
.editorconfig
.clang-format
.gitignore
CMakePresets.json
```

Component-specific configuration should remain local when it does not affect the whole project.

---

## 56. Documentation Ownership

Documentation should follow the architecture it describes.

```text
System Architecture
    → ARCHITECTURE.md

Repository Architecture
    → docs/architecture/REPOSITORY_ARCHITECTURE.md

Core Architecture
    → core/ARCHITECTURE.md

SoC Architecture
    → docs/architecture/SOC_ARCHITECTURE.md

Verification
    → docs/verification/
```

Avoid duplicating the same architecture in multiple documents.

---

## 57. Single Source of Truth

The repository should establish clear sources of truth.

| Topic | Canonical Location |
|---|---|
| System architecture | `ARCHITECTURE.md` |
| Repository architecture | `docs/architecture/REPOSITORY_ARCHITECTURE.md` |
| Requirements | `docs/requirements/` |
| Public APIs | `docs/api/` and component headers |
| Verification | `docs/verification/` |
| Architecture decisions | `docs/adr/` |
| Project overview | `README.md` |

---

## 58. Avoid Documentation Drift

When an architectural boundary changes:

1. Update the canonical architecture.
2. Update repository architecture.
3. Update requirements if necessary.
4. Update implementation.
5. Update tests.
6. Update related documentation.

Architecture documentation should not describe structures that no longer exist.

---

## 59. Integration Branching

The exact Git branching strategy may evolve.

The repository should support:

```text
main
  │
  ├── feature/*
  ├── hardware/*
  ├── simulation/*
  └── release/*
```

The project should prefer short-lived branches and frequent integration once interfaces are stable.

---

## 60. Tagging

Releases should be tagged.

Example:

```text
v0.1.0
v0.2.0
v1.0.0
```

Hardware revisions may use related identifiers.

---

## 61. Security of the Repository

Repository security should include:

- Protected branches
- Review requirements
- Dependency scanning
- Secret scanning
- Signed releases where appropriate
- Controlled hardware/IP repositories
- Access control

Commercial IP should not accidentally be committed to public repositories.

---

## 62. Open Source Boundary

The public KritvaOS repository can contain open software and interfaces.

Future proprietary components may include:

- Commercial hardware
- Silicon implementations
- Specialized accelerators
- Customer-specific IP
- Production security components

The boundary should be explicitly documented before proprietary material is introduced.

---

## 63. Licensing Boundary

The repository should clearly identify licensing for:

```text
Open Source
Third-Party
Commercial
Customer-Specific
Generated
```

Every future independent repository should have its own licensing documentation where required.

---

## 64. Third-Party Dependencies

Third-party dependencies should be:

- Explicit
- Version-pinned or bounded
- License-reviewed
- Security-reviewed
- Reproducible

A dependency should not become a hidden architectural requirement.

---

## 65. ROS2 Repository Boundary

ROS2 integration should not force the entire Kritva repository to depend on ROS2.

Preferred model:

```text
Kritva Core
     │
     ├── Native Kritva interfaces
     │
     └── ROS2 Integration Layer
              │
             ROS2
```

This keeps Kritva architecture independent while supporting the ROS2 ecosystem.

---

## 66. EtherCAT Repository Boundary

EtherCAT should be treated as a communication technology within the hardware and real-time architecture.

Possible organization:

```text
drivers/ethercat/
soc/edge/
tests/ethercat/
```

The exact implementation may later become an independent repository if warranted.

---

## 67. Simulation Repository Boundary

Simulation is a cross-layer concern.

Initially:

```text
sim/
```

may remain in the main repository.

Later it may become:

```text
kritva-sim
```

if independent development and release justify the split.

---

## 68. FPGA Repository Boundary

FPGA assets may initially live with their associated SoC:

```text
soc/nexus/fpga/
soc/edge/fpga/
```

If FPGA development becomes independently reusable, it may move to:

```text
kritva-fpga
```

or dedicated platform repositories.

---

## 69. Silicon / IP Repository Boundary

Silicon development may require stronger access control.

Potential future organization:

```text
KritvaOS
    → Open software

Kritva-HW
    → Reference hardware

Kritva-IP
    → Commercial IP

Kritva-Silicon
    → Silicon implementation
```

This is a future possibility, not an immediate requirement.

---

## 70. Customer Program Repositories

Customer-specific development should avoid contaminating the common open repository.

Potential model:

```text
KritvaOS
    │
    ├── Common platform
    │
    └── Customer integration repository
              │
              ├── Customer configuration
              ├── Customer hardware
              └── Customer application
```

Reusable improvements should be evaluated for upstream contribution.

---

## 71. Reference Hardware

Reference hardware should be versioned separately from customer-specific hardware where practical.

Example:

```text
hardware/
├── reference/
│   ├── nexus/
│   └── edge/
└── customer/
```

Customer material should not be placed in a public repository without appropriate authorization.

---

## 72. Robot Definitions

Robot-specific configuration can be organized as:

```text
robots/
├── humanoid/
├── manipulator/
├── mobile/
├── quadruped/
└── aerial/
```

This directory is optional initially.

Robot-specific software should remain in the appropriate module rather than creating large robot-specific forks.

---

## 73. Deployment Artifacts

Deployment artifacts may include:

```text
images/
packages/
firmware/
containers/
models/
configs/
```

These should be generated or released through controlled build pipelines rather than manually committed binaries.

---

## 74. Containerization

Containers may be used for:

- Development
- CI
- Simulation
- AI environments
- Toolchains

Containers should not be assumed to be the deployment mechanism for hard real-time components.

---

## 75. Reproducible Development Environment

The repository should eventually provide:

```text
Dockerfile
docker/
devcontainer/
toolchain/
```

where useful.

The goal is to allow a new developer to reproduce the supported build and test environment.

---

## 76. Cross-Compilation

The repository should support target-specific toolchains as hardware matures.

Potential targets:

```text
x86
ARM
RISC-V
MCU
FPGA
```

Toolchain configuration should be isolated from application source code.

---

## 77. Hardware Build Separation

Hardware build artifacts should not be mixed with software build artifacts.

Example:

```text
build/
├── software/
└── hardware/
```

Actual CI/build implementation may use separate workspaces.

---

## 78. Traceability Across Repository

A complete change should be traceable:

```text
Requirement
    ↓
ADR / Architecture
    ↓
Directory
    ↓
Source File
    ↓
API
    ↓
Test
    ↓
Verification Result
```

This is particularly important for Core, real-time, safety, security, and hardware interfaces.

---

## 79. Initial Implementation Priority

Recommended implementation order:

```text
1. Repository skeleton
2. Core Foundation
3. Core tests
4. Hardware abstraction
5. Minimal Endpoint
6. Minimal Edge
7. Nexus interface
8. Communication path
9. Simulation
10. Renode
11. First vertical slice
12. Sense / Motion integration
13. Skill
14. Mind
15. SDK
```

The exact ordering can change based on the first customer or hardware target.

---

## 80. First Vertical Slice Repository Path

The first complete path should be easy to locate.

Conceptually:

```text
examples/
    robot_vertical_slice/

core/
hardware/
drivers/
soc/
    nexus/
    edge/
tests/
    integration/
    renode/
```

The objective is to prove:

```text
Application
 → Skill
 → Core
 → Nexus
 → EtherCAT
 → Edge
 → Motor + Encoder
```

and return feedback through the same system.

---

## 81. Repository Success Criteria

The repository architecture is successful when:

1. Developers can identify where a capability belongs.
2. Dependencies have a predictable direction.
3. Components can be developed independently where appropriate.
4. Cross-layer integration remains straightforward.
5. Tests are located according to responsibility.
6. Simulation can reuse production interfaces.
7. Hardware and software development can proceed in parallel.
8. Future repository splits can occur without redesigning the architecture.
9. Customer-specific work can remain isolated.
10. The source tree reflects the system architecture.

---

## 82. Anti-Patterns

The repository should avoid:

### 82.1 Everything in Core

Do not put drivers, robotics algorithms, AI, or applications into Core.

### 82.2 Hardware Leakage

Do not expose vendor-specific hardware types throughout the software stack.

### 82.3 Premature Micro-Repositories

Do not create many repositories before interfaces stabilize.

### 82.4 Duplicate APIs

Do not create competing definitions of the same public interface.

### 82.5 Test Duplication

Do not copy the same integration test into multiple component directories.

### 82.6 Customer Fork Explosion

Do not permanently fork the common platform for every customer.

### 82.7 Documentation Duplication

Do not maintain multiple conflicting architecture documents.

---

## 83. Repository Evolution

The repository may evolve through:

```text
Phase 1
Monorepo
   ↓
Phase 2
Stable Component Boundaries
   ↓
Phase 3
Selected Git Submodules
   ↓
Phase 4
Independent Platform Repositories
   ↓
Phase 5
Open + Commercial Repository Ecosystem
```

The transition should be driven by engineering needs rather than organizational fashion.

---

## 84. Recommended Long-Term Repository Model

A possible long-term structure is:

```text
KritvaOS
├── Core
├── Sense
├── Mind
├── Motion
├── Skill
└── SDK

Kritva-Sim
└── Simulation / Digital Twin

Kritva-Nexus
├── RTL
├── FPGA
├── Firmware
└── Verification

Kritva-Edge
├── RTL
├── FPGA
├── Firmware
└── Verification

Kritva-Hardware
└── Reference Platforms

Kritva-IP
└── Commercial Robotics Compute IP
```

This is a target architecture, not a requirement for the current pre-alpha repository.

---

## 85. Final Repository Principle

The repository should reflect the architecture without becoming a constraint on it.

> **Keep the architecture stable, keep implementation modular, and split repositories only when the boundary has earned the split.**

The initial Kritva strategy is therefore:

```text
Common Architecture
       ↓
Modular Monorepo
       ↓
Stable Interfaces
       ↓
Validated Components
       ↓
Selective Git Submodules
       ↓
Independent Repositories Where Justified
```

This provides a practical path from an early open-source project to a larger robotic computing platform.

---

## Status

**Repository Architecture Status:** Early Architecture / Pre-Alpha

This document should evolve together with `ARCHITECTURE.md`, requirements, APIs, verification plans, and ADRs.


## KOS-I3 Hardware Abstraction Placement

KOS-I3 introduces the first concrete use of the repository's hardware abstraction area.

Recommended initial placement:

```text
hardware/
└── abstraction/
    ├── include/
    │   └── kritva/
    │       └── hardware/
    │           ├── device.hpp
    │           ├── endpoint.hpp
    │           ├── sensor_endpoint.hpp
    │           ├── actuator_endpoint.hpp
    │           └── device_registry.hpp
    ├── src/
    └── CMakeLists.txt
```

Mock implementations belong with I3 verification support:

```text
tests/hardware/mock/
```

The reference executable belongs under:

```text
examples/device_demo/
```

This placement does not imply that every future Device/Endpoint implementation belongs in one directory. Physical drivers remain under `drivers/`, while platform-specific implementations may live under `soc/edge/` or another appropriate platform area.

The repository must preserve the following dependency direction:

```text
Runtime
  ↓
Hardware Abstraction
  ↓
Driver / Platform
  ↓
Hardware
```

The abstraction must not depend upward on Motion, Mind, Skill, or application code.
