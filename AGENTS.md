# Kritva Engineering Agent Instructions

## 1. Purpose

This document defines the engineering rules and operating principles for
AI coding agents working in the Kritva repository.

These instructions apply to the entire repository unless a more specific
`AGENTS.md` exists in a child directory.

AI agents must treat this document together with the canonical Kritva
architecture, requirements, repository structure, toolchain definition,
and verification requirements.

Kritva is an Open Robotic Computing Platform.

KritvaOS is the open robotic software platform within the broader Kritva
architecture.

The goal is to connect Physical AI to physical action through an open,
hardware-aware, distributed robotic computing architecture.

Kritva's primary architectural principle is:

> Kritva connects intelligence to physical action.

---

## 2. Canonical Engineering References

Before making architectural or cross-component changes, agents should
consult the following sources in this order:

1. `ARCHITECTURE.md`
2. `docs/architecture/`
3. `docs/requirements/`
4. `docs/api/`
5. `docs/verification/`
6. `docs/architecture/REPOSITORY_ARCHITECTURE.md`
7. `toolchain/VERSIONS.yaml`
8. Relevant component `README.md`
9. Relevant ADRs under `docs/adr/`

The root `ARCHITECTURE.md` is the canonical system architecture.

The repository architecture document defines where functionality belongs.

`toolchain/VERSIONS.yaml` is the source of truth for supported development
tools and versions.

Do not silently contradict these documents.

If an implementation appears to require an architectural change, propose
the architectural change rather than silently changing the architecture
through code.

---

## 3. Kritva Architectural Identity

Kritva is not merely:

- a ROS distribution
- a robot application framework
- a humanoid operating system
- a robot OEM stack
- an AI model platform
- a Linux distribution
- a semiconductor company

Kritva is an Open Robotic Computing Platform.

Kritva connects:

Physical AI
    ↓
Robotic Intelligence
    ↓
Skills and Behavior
    ↓
Perception and Motion
    ↓
Real-Time Computing
    ↓
Distributed Compute
    ↓
Physical Endpoints
    ↓
Physical World

The architectural principle is:

> Kritva connects intelligence to physical action.

---

## 4. System Architecture

The common Kritva computing stack is:

Application
    ↓
Kritva SDK
    ↓
Kritva Skill
    ↓
Kritva Mind / Kritva Motion
    ↓
Kritva Sense
    ↓
Kritva Core
    ↓
Hardware Abstraction
    ↓
Nexus
    ↓
Edge
    ↓
Subnode
    ↓
Endpoint
    ↓
Physical World

Do not introduce shortcuts that unnecessarily bypass these architectural
boundaries.

Cross-layer dependencies require architectural justification.

---

## 5. Kritva Software Components

### Kritva Core

Core is the platform foundation.

Core Foundation includes concepts such as:

- Identity
- Lifecycle
- Status
- Health
- Statistics
- Error / Fault
- Result
- Events
- Capability
- Configuration
- Version
- Timestamp
- Duration
- Metadata

Core must remain platform-independent.

Do not put robotics algorithms, ROS2 dependencies, EtherCAT implementations,
cloud services, database dependencies, vendor-specific drivers, or robot
application logic into Core Foundation unless explicitly architected.

### Kritva Sense

Responsible for sensor integration and perception-related interfaces.

Examples include:

- cameras
- IMUs
- LiDAR
- force/torque sensors
- encoders
- tactile sensors

Sensor-specific implementation should remain outside Core Foundation.

### Kritva Mind

Responsible for intelligence and higher-level reasoning.

Examples include:

- AI inference
- planning
- world models
- reasoning
- decision making
- task planning

Mind must not directly depend on physical hardware implementation.

### Kritva Motion

Responsible for physical motion and control.

Examples include:

- locomotion
- manipulation
- kinematics
- dynamics
- balance
- trajectory generation
- motion planning
- control
- joint coordination

Real-time constraints must be explicitly considered for motion/control code.

### Kritva Skill

Responsible for reusable robot capabilities and behaviors.

Skills should compose lower-level capabilities rather than duplicate them.

### Kritva Sim

Responsible for simulation and validation.

Examples include:

- simulation models
- digital twins
- software-in-the-loop
- hardware-in-the-loop
- regression environments
- Renode
- FPGA validation environments

Simulation should reuse the same interfaces as physical implementations
where practical.

### Kritva SDK

Provides customer and developer-facing interfaces.

SDK APIs must be treated as externally consumable interfaces and therefore
require stronger compatibility discipline.

---

## 6. Hardware Computing Hierarchy

Kritva uses the following physical computing hierarchy:

Nexus
    ↓
Edge
    ↓
Subnode
    ↓
Endpoint

### Nexus

Nexus represents the system-level compute platform.

Typical responsibilities include:

- high-level compute
- AI acceleration
- system orchestration
- global services
- system management
- storage
- security
- network communication
- EtherCAT system-level coordination

Nexus is a platform family, not necessarily one fixed silicon design.

### Edge

Edge represents distributed real-time compute.

Typical responsibilities include:

- motor control
- joint control
- encoder processing
- deterministic I/O
- PWM
- ADC
- SPI
- I2C
- UART
- GPIO
- EtherCAT communication

Edge is a platform family, not necessarily one fixed silicon design.

### Subnode

Subnodes provide localized distributed processing close to sensors
or actuators.

### Endpoint

Endpoints represent physical interfaces to:

- sensors
- actuators
- motors
- encoders
- switches
- physical devices

---

## 7. Product Naming Rules

Use:

- Kritva Nexus SoC
- Kritva Edge SoC
- Nexus platform
- Edge platform
- Subnode
- Endpoint

Avoid treating "Master" and "Node" as Kritva product names.

"Master" and "Node" may be used as technical protocol terminology
when required by a specific technology.

---

## 8. Robot-Type Independence

Kritva is robot-type agnostic at the platform level.

Potential systems include:

- humanoids
- manipulators
- cobots
- mobile robots
- quadrupeds
- aerial robots
- future robotic systems

Do not redesign the entire Kritva architecture for each robot type.

Instead:

Common Kritva Platform
    +
Robot-Specific Sense
    +
Robot-Specific Motion
    +
Robot-Specific Edge / Hardware
    =
Robot Implementation

Robot-specific functionality belongs in appropriate robot, sense, motion,
hardware, or deployment layers.

---

## 9. ROS2 and External Ecosystems

Kritva does not require replacing the broader robotics ecosystem.

ROS2 may be used for:

- ecosystem integration
- application integration
- robotics middleware
- interoperability

`ros2_control` may be reused where appropriate.

micro-ROS may be used for constrained MCU environments where appropriate.

Do not introduce ROS2 dependencies into Kritva Core merely because ROS2 is
used elsewhere in the system.

Prefer integration over unnecessary reimplementation.

---

## 10. Real-Time Engineering

Real-time behavior is an architectural property.

When implementing real-time code, explicitly consider:

- deterministic execution
- bounded latency
- scheduling
- priority
- interrupt behavior
- memory allocation
- synchronization
- lock contention
- queue behavior
- communication latency
- jitter
- fault handling

Do not assume that a function is real-time safe merely because it executes
quickly during a normal test.

Avoid uncontrolled:

- dynamic allocation
- blocking I/O
- unbounded loops
- blocking locks
- filesystem operations
- network operations
- logging operations

inside hard real-time paths unless explicitly justified.

Linux with PREEMPT_RT may be used where appropriate.

MCU/RTOS environments may be used for constrained deterministic nodes.

---

## 11. Safety

Robotic software controls physical systems.

Safety must therefore be considered whenever code can influence:

- motors
- actuators
- brakes
- power
- joint movement
- robot state
- emergency behavior
- safety limits

Agents must not remove or weaken safety checks merely to make tests pass.

If a proposed change affects safety behavior, identify it explicitly.

Safety-critical changes should receive human review.

---

## 12. Security

Security is a system property spanning:

- boot
- identity
- authentication
- authorization
- communication
- firmware
- storage
- updates
- diagnostics
- AI models
- physical interfaces

Do not introduce:

- hard-coded credentials
- secrets
- private keys
- tokens
- production certificates
- insecure default credentials

Never commit secrets into the repository.

Security-sensitive changes require explicit review.

---

## 13. Hardware Abstraction

Hardware-specific functionality must be isolated behind defined interfaces.

Do not allow:

vendor-specific hardware assumptions
        ↓
Kritva Core API

Instead:

Hardware
    ↓
Hardware Abstraction
    ↓
Kritva Interfaces
    ↓
Kritva Software

Hardware abstraction should enable the same higher-level software to work
across simulation, FPGA, development hardware, and future silicon where
practical.

---

## 14. Simulation First Where Practical

When introducing hardware-dependent functionality, prefer validating the
interface before requiring physical hardware.

Preferred development progression:

Requirements
    ↓
Architecture
    ↓
API
    ↓
Software / RTL
    ↓
Simulation
    ↓
Renode
    ↓
FPGA
    ↓
HIL
    ↓
Silicon
    ↓
Robot

Do not make physical hardware a prerequisite for functionality that can
reasonably be validated in simulation.

---

## 15. API-First Engineering

Before implementing a new cross-component capability:

1. Identify the requirement.
2. Identify the architectural owner.
3. Define the API/interface.
4. Define expected behavior.
5. Define error behavior.
6. Define test cases.
7. Implement.
8. Verify.

Avoid beginning with implementation details when the interface is not
defined.

---

## 16. Requirement Traceability

Kritva follows:

Requirement
    ↓
Architecture
    ↓
API
    ↓
Implementation
    ↓
Test
    ↓
Verification Result

For meaningful functionality, agents should be able to identify the
requirement or design rationale behind the implementation.

Do not create functionality solely because it "might be useful" without
understanding where it belongs.

---

## 17. Testing Requirements

New functionality should include appropriate tests.

Preferred progression:

Unit Test
    ↓
Contract Test
    ↓
Integration Test
    ↓
Simulation
    ↓
Hardware / HIL
    ↓
System Validation

At minimum, code should not be considered complete merely because it
compiles.

Tests should verify:

- normal behavior
- boundary conditions
- invalid input
- failure behavior
- recovery behavior where applicable
- interface contracts

---

## 18. CI Requirements

The canonical development interface is the repository Makefile.

Use:

```text
make ci
```

for the standard CI validation path.

The development environment is:

```text
kritvaos-dev:0.1
```

Do not silently substitute arbitrary host tool versions when validating
repository changes.

The source of truth for tool versions is:

```text
toolchain/VERSIONS.yaml
```

CI should use the Kritva-defined toolchain.

---

## 19. Toolchain Discipline

Developers may use different editors and IDEs.

However, repository:

- builds
- tests
- formatting
- static analysis
- simulation
- release preparation

must use the Kritva-defined toolchain.

Do not modify tool versions casually.

Changes to the baseline toolchain require:

1. rationale
2. compatibility assessment
3. validation
4. changelog update
5. ADR when the change is architecturally significant

---

## 20. C++ Standards

Kritva C++ code currently targets:

```text
C++20
```

Prefer:

- clear ownership
- RAII
- strong types
- explicit interfaces
- const correctness
- predictable behavior
- testable components

Avoid unnecessary abstractions.

Do not introduce a large framework dependency for a small utility.

---

## 21. Dependency Discipline

Before introducing a dependency, evaluate:

- necessity
- licensing
- maintenance
- portability
- security
- build complexity
- runtime overhead
- real-time implications
- target-platform support

Prefer existing Kritva dependencies and standard-library functionality
where practical.

Do not add a dependency merely because it makes one implementation
slightly easier.

---

## 22. Repository Boundaries

Follow:

docs/        → what Kritva is
source dirs  → how Kritva is implemented
project/     → what is being built and when
.github/     → collaboration and automation
toolchain/   → development environment
tests/       → verification

Do not move files across architectural boundaries merely to make an
implementation convenient.

When uncertain where functionality belongs, propose the boundary before
implementing it.

---

## 23. Monorepo and Git Submodules

KritvaOS uses a hybrid repository architecture.

The top-level KritvaOS repository contains the common platform integration and
may integrate selected components as Git submodules. A directory must NOT be
assumed to belong to the top-level repository solely because it exists under
the KritvaOS directory.

### Current Git Submodule Map

The following table is the authoritative documentation of the intended current
submodule layout. It MUST be kept synchronized with `.gitmodules`.

| Path | Component | Repository | Status |
|---|---|---|---|
| `<actual-submodule-path>` | `<component>` | `<repository>` | Git submodule |

If no submodules are currently configured, this table MUST explicitly state:

> No Git submodules are currently configured.

Do not list a directory as a current Git submodule unless it is actually
configured as one in `.gitmodules`.

### Git ownership verification

Before modifying files under a component directory, agents MUST determine
whether the directory is:

- owned by the parent KritvaOS repository
- a Git submodule
- an independent repository not currently integrated
- unknown

For this determination, inspect:

```text
.gitmodules
git submodule status
git status
```

When appropriate, inspect the Git metadata of the component directory as well.

If documentation and actual Git state disagree, report the discrepancy before
modifying files.

### Submodule rules

For a Git submodule:

- Treat the submodule as an independently owned repository.
- Do not modify the submodule from a parent-repository task unless explicitly
  requested.
- Do not change the submodule commit recorded by the parent repository unless
  explicitly requested.
- Do not create, delete, rename, or reorganize files inside a submodule as
  part of a parent-repository task unless the task explicitly includes that
  submodule.
- Do not initialize, update, switch, or change submodule branches without
  explicit approval.
- Do not automatically commit submodule changes from the parent repository.
- Before modifying a submodule, report:
  - submodule path
  - repository URL
  - current commit
  - branch/detached state
  - working-tree status
  - whether the parent repository pointer will change

### Updating a submodule

When a submodule is intentionally changed:

1. Work in the submodule repository.
2. Read and follow its own `AGENTS.md` and repository instructions.
3. Implement and validate the change in the submodule repository.
4. Commit the submodule repository separately.
5. Update the parent repository's submodule pointer.
6. Validate the parent repository.
7. Report both the submodule commit and parent-repository pointer change.

### Repository splitting

Do not create a new repository or submodule merely to separate a small
component.

Repository splitting should be justified by factors such as:

- independent ownership
- independent release lifecycle
- reusable external product
- licensing requirements
- hardware/vendor boundary
- build independence
- organizational scaling

Until an actual split is approved and configured, treat the component as part
of the existing KritvaOS repository.

---

## 24. Documentation Requirements

Architectural changes must update the appropriate documentation.

Examples:

System architecture
    → `ARCHITECTURE.md`

Repository structure
    → `docs/architecture/REPOSITORY_ARCHITECTURE.md`

Architecture decision
    → `docs/adr/`

Requirements
    → `docs/requirements/`

API
    → `docs/api/`

Verification
    → `docs/verification/`

Project planning
    → `project/`

Do not duplicate the same architectural truth across many documents.

---

## 25. ADR Requirements

An ADR should be considered when a change affects:

- architecture
- component boundaries
- public API strategy
- communication architecture
- real-time strategy
- hardware/software boundary
- SoC architecture
- repository architecture
- external middleware strategy
- major dependencies
- licensing
- security architecture
- safety architecture

Do not silently encode major architectural decisions in implementation code.

---

## 26. AI Agent Behavior

AI agents must behave as engineering assistants, not autonomous architects.

Agents should:

- inspect existing code before changing it
- understand the architecture before implementing
- minimize unnecessary changes
- preserve existing interfaces unless change is justified
- identify assumptions
- identify uncertainty
- explain architectural tradeoffs
- add tests
- run relevant validation
- report failures honestly
- avoid hiding warnings
- avoid speculative implementation

Agents must not:

- rewrite large areas unnecessarily
- introduce unrelated refactoring
- change public APIs without justification
- remove tests to make CI pass
- weaken validation
- bypass safety checks
- bypass security controls
- silently change architecture
- invent unavailable hardware behavior
- claim tests passed when they were not executed

---

## 27. Change Scope

Prefer small, reviewable changes.

A change should ideally answer:

> What problem does this change solve?

Avoid combining unrelated changes such as:

feature implementation
+
architecture redesign
+
formatting entire repository
+
dependency migration
+
unrelated refactoring

in one change.

Separate them unless there is a clear dependency.

---

## 28. Before Editing

Before modifying code, agents should:

1. Inspect the relevant files.
2. Identify the owning component.
3. Check existing interfaces.
4. Check relevant requirements.
5. Check relevant tests.
6. Check relevant architecture documentation.
7. Identify dependencies.
8. Determine whether an ADR is required.

Do not edit first and understand later.

---

## 29. After Editing

After implementation:

1. Review the diff.
2. Build the affected component.
3. Run relevant tests.
4. Run formatting checks.
5. Run static analysis when applicable.
6. Run `make ci` when appropriate.
7. Check for unintended changes.
8. Update documentation.
9. Update changelog when the change is externally or architecturally
   meaningful.

---

## 30. Git Discipline

Never perform destructive Git operations unless explicitly requested.

Avoid:

```text
git reset --hard
git clean -fd
git push --force
```

unless the user explicitly authorizes the operation.

Do not rewrite another developer's commits.

Prefer small logical commits.

Commit messages should describe the engineering intent.

Examples:

```text
feat(core): add lifecycle foundation
test(core): add lifecycle contract tests
fix(core): correct result error propagation
docs: define repository architecture
ci: update GitHub checkout action
chore: establish v0.1 toolchain baseline
```

---

## 31. Pull Request Expectations

A pull request should clearly describe:

- problem
- solution
- affected components
- architecture impact
- API impact
- testing
- verification
- known limitations
- safety impact where applicable
- security impact where applicable

For architecture-impacting changes, include the relevant ADR.

---

## 32. Human Review Boundaries

Human review is required for changes involving:

- safety-critical behavior
- security architecture
- boot/security chain
- physical actuator control
- public API compatibility
- SoC architecture
- RTL affecting silicon behavior
- clock/reset architecture
- power architecture
- memory protection
- distributed synchronization
- licensing
- major repository restructuring

AI agents may propose and implement such changes, but must clearly flag
them for human review.

---

## 33. No Hallucinated Hardware

Never assume that a hardware feature exists.

Do not invent:

- registers
- memory maps
- interrupts
- clocks
- buses
- DMA channels
- hardware accelerators
- sensor interfaces
- EtherCAT capabilities
- SoC peripherals

If hardware specifications are unavailable, explicitly mark the assumption
or request the relevant specification.

---

## 34. No Hallucinated APIs

Before using an external API or library:

- inspect the installed/documented version
- verify the API exists
- verify the expected version
- prefer authoritative documentation

Do not generate code based solely on remembered API names when verification
is possible.

---

## 35. External Technology Integration

Kritva should integrate with mature external technologies where appropriate.

Examples include:

- Linux
- PREEMPT_RT
- ROS2
- ros2_control
- micro-ROS
- EtherCAT
- Renode
- Verilator
- CMake
- Clang
- existing AI frameworks

The default strategy is:

> Reuse where appropriate. Abstract where necessary. Reimplement only
> where Kritva has a clear architectural or IP reason.

---

## 36. Intellectual Property

Kritva should distinguish between:

Open-source infrastructure
    ↓
Kritva architecture
    ↓
Kritva software
    ↓
Kritva hardware architecture
    ↓
Kritva reusable IP
    ↓
Customer-specific IP

Do not accidentally introduce proprietary or incompatible material into
open-source Kritva repositories.

Check licensing before copying code, documentation, RTL, models, or other
external material.

---

## 37. Customer-Specific Work

Customer-specific functionality must not unnecessarily contaminate the
common Kritva platform.

Prefer:

Common Kritva Platform
        +
Customer Adapter / Extension
        +
Customer-Specific Implementation

rather than modifying the common platform for one customer unless the
capability has broader platform value.

---

## 38. Architecture Evolution

Kritva is an evolving platform.

Early implementations should optimize for:

- clear interfaces
- learning
- validation
- modularity
- portability
- traceability

Avoid premature optimization for hypothetical future requirements.

At the same time, avoid architectural shortcuts that would prevent future
Nexus, Edge, FPGA, silicon, or robotic-system evolution.

---

## 39. First Vertical Slice

The highest-priority architectural proof is a complete vertical slice:

Application
    ↓
Skill
    ↓
Mind interface
    ↓
Kritva Core
    ↓
Nexus
    ↓
EtherCAT
    ↓
Edge
    ↓
Motor + Encoder

This slice should prove that Kritva can connect intelligence to physical
action.

Do not allow broad platform development to indefinitely delay this proof.

---

## 40. Current v0.1 Priority

The immediate engineering priorities are:

1. Repository foundation
2. Toolchain reproducibility
3. CMake build foundation
4. Kritva Core Foundation
5. Unit and contract testing
6. CI
7. Hardware abstraction
8. Nexus / Edge interfaces
9. Simulation
10. First vertical slice

Do not prematurely implement the complete robotic ecosystem.

---

## 41. Engineering Principle

When choosing between two technically valid solutions, prefer the solution
that:

1. preserves architectural boundaries
2. reduces unnecessary coupling
3. keeps interfaces stable
4. is testable
5. is deterministic where required
6. is portable across compute platforms
7. supports simulation
8. minimizes unnecessary dependencies
9. preserves future hardware freedom
10. is understandable by future engineers

---
## 42. Git Repository and Submodule Policy

KritvaOS uses a hybrid repository architecture.

The top-level KritvaOS repository integrates selected components as Git submodules.
A directory must NOT be assumed to be part of the top-level repository solely because
it exists under the KritvaOS directory.

### 43. Git ownership

Before modifying files under a component directory:

1. Determine whether the directory is a Git submodule.
2. Check `.gitmodules`.
3. Check Git status from the component directory.
4. Identify the owning repository and current commit.
5. Respect the component repository's own `AGENTS.md` and development instructions.

### 44. Submodule rules

For a Git submodule:

- Treat the submodule as an independently owned repository.
- Do not modify the submodule from the parent repository unless explicitly requested.
- Do not change the submodule commit recorded by the parent repository unless explicitly requested.
- Do not create, delete, rename, or reorganize files inside a submodule as part of
  a parent-repository task unless the task explicitly includes that submodule.
- Do not commit submodule changes to the parent repository automatically.
- Do not initialize, update, switch, or change submodule branches without explicit approval.
- Before modifying a submodule, report:
  - submodule path
  - repository URL
  - current commit
  - branch/detached state
  - working-tree status

### 45. Parent repository changes

When a submodule is intentionally updated:

1. Make and validate the changes in the submodule repository.
2. Commit the submodule repository separately.
3. Update the parent repository's submodule pointer.
4. Validate the parent repository.
5. Report both commits clearly.

### 46. Unknown ownership

If ownership of a directory is unclear, stop before modifying it and report the
detected Git structure.

Never assume that a normal directory is a parent-repository directory.
---

## 47. Final Rule

The most important rule for every AI agent working on Kritva is:

> Understand the architecture before changing the code.

And the primary architectural objective is:

> One Kritva architecture.
> Multiple robotic systems.
> Multiple compute implementations.
> Stable interfaces.
> Reusable software and hardware IP.
> Open ecosystem.
> Validated from simulation to physical robot.

Kritva connects Physical AI to the physical world.

## 48. Program Governance Rules

These rules apply in addition to everything above and follow the program's governance (D-035, D-040).

- Never commit credentials, tokens, private keys or production data. Use obviously fake values in examples. The `secret-scan` workflow runs on every push and pull request; do not add allowlist entries to silence it, propose them to the owner.
- Do not add `Claude-Session:` or session-URL trailers to commit messages or pull request descriptions; `commit-message-check` rejects them. A `Co-Authored-By:` line is allowed.
- Commit and PR standard (kritva-program D-053, v2/planning/ACCEPTANCE_STANDARD.md section 4): `type(scope): summary` (types feat, fix, docs, refactor, test, build, ci, chore, perf, revert, security) with one `Task:` or `Tasks:` footer, `Tasks: None (reason)` only on a docs or chore commit that implements no task; the merge method is a merge commit; PR text follows `.github/PULL_REQUEST_TEMPLATE.md`. Check with `python3 scripts/security/check_commit_format.py --range origin/main..HEAD`. This repository adopted the format under D-061: `commit-message-check` runs this check on every pull request.
- Work on a branch and open a pull request; do not push to `main`, force-push shared branches or rewrite history without the owner's explicit approval.
- `status/status.json` is a generated planning report. Regenerate it, do not edit it by hand. Repository edits never create program credit: credit comes only from the baseline, approvals and evidence recorded in `KritvaOS/kritva-program`.
- Program decisions are recorded in `KritvaOS/kritva-program` (`v2/DECISIONS.md`). Dinesh decides; AI advice is advisory.
- `main` requires `secret-scan`, `commit-message-check` and Code Owner review (owner statement, D-040; not independently verified). With one owner the PR author cannot self-approve: use the owner bypass or a second reviewer.

## 49. README changes

Run `make check-readme` after editing README.md. It is a structural check only (metadata fields, topic headings, relative links, placeholders); it does not show that the content is accurate, so review the content against the repository as well.
