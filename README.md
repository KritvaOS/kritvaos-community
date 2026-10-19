<!--
Copyright (c) 2026 KritvaOS
SPDX-License-Identifier: Apache-2.0

File        : README.md
Description : Repository overview and navigation map (Kritva README standard)

Component   : KritvaOS
Module      : Documentation
Layer       : Documentation

Author      : KritvaOS Development Team
Created     : 19-10-2026
-->

# KritvaOS Community

> The integration repository of the KritvaOS Open Robotic Computing Platform: it assembles the Kritva Core submodule, the KOS-I2 application runtime, the KOS-I3 hardware abstraction with mock devices and the KOS-I4 remote (Nexus and Edge) layer, all as software verified on a host computer. It is pre-alpha. The visibility class of this repository is PUBLIC (D-063).

| Property | Value |
|---|---|
| Project | KritvaOS |
| Repository | `KritvaOS/kritvaos-community` |
| Repository Type | Platform integration (application runtime, hardware abstraction and remote layer; parent of the `core` submodule) |
| Visibility Class | PUBLIC (owner decision D-063 in kritva-program; it is an exception to the D-039 recommendation of PRIVATE, and no release review is recorded) |
| GitHub Setting | PUBLIC, the same as the class (D-063 in kritva-program keeps this repository public and takes it out of the return-to-private plan); OWNER-ATTESTED, UNVERIFIED: no independent check of the setting and no verification date is recorded here |
| Lifecycle Status | ACTIVE (pre-alpha) |
| Repository Owner | Project owner (role; see section 15) |
| Security Owner | Security Owner role ([SECURITY.md](SECURITY.md)) |
| Architecture | [ARCHITECTURE.md](ARCHITECTURE.md) and [docs/architecture/](docs/architecture/) |
| Milestones | [planning/MILESTONE.md](planning/MILESTONE.md), [planning/MILESTONE_STATUS.md](planning/MILESTONE_STATUS.md) and [planning/milestones/](planning/milestones/) |
| Tasks | [planning/TASKS.md](planning/TASKS.md) and the `TASKS.md` of each milestone under [planning/milestones/](planning/milestones/) |
| Acceptance Criteria | `ACCEPTANCE_CRITERIA.md` in each milestone folder under [planning/milestones/](planning/milestones/) |
| Status Reporting | [status/README.md](status/README.md) and [status/status.json](status/status.json) (generated) |
| License | [LICENSE](LICENSE) (Apache License 2.0) |

This README follows the Kritva README standard (`v2/planning/standards/README_STANDARD.md` in [kritva-program](https://github.com/KritvaOS/kritva-program)). It is a navigation map; the documents it links own the detail. The long vision text that used to be the README is kept as [docs/architecture/KRITVA_VISION.md](docs/architecture/KRITVA_VISION.md); it describes intent, not implemented state.

## 1. Purpose and Scope

Kritva is intended to be an open, hardware-aware robotic computing platform that connects Physical AI to physical action. This repository is where the platform's software parts are assembled and exercised together. It holds the application runtime, the hardware abstraction, mock devices, the transport and remote layer, examples, tests, the toolchain definition and the planning records for the KOS-I2, KOS-I3 and KOS-I4 milestones.

**This repository owns** the application runtime (`runtime/`), the hardware abstraction, mock endpoints, transport and remote layer (`hardware/`), the examples, the integration tests, the top-level CMake build, the toolchain definition and its own planning and status records.

**This repository does not own** the Core library (it lives in `kritva-core` and is included as the `core` submodule), the program plan, baseline and approvals (kritva-program), real hardware drivers, any robot algorithm, or actuator hardware, firmware and mechanics (the kritva-actuator repositories).

**Implementation state.** IMPLEMENTED (as host software): the KOS-I2 runtime host, component composition, configuration, observation and failure handling; the KOS-I3 device and endpoint abstraction, registry, typed sensor and actuator endpoints, mock IMU, motor and endpoint, and diagnostics; the KOS-I4 frame format, protocol codec, deterministic simulated transport, `EdgeHost`, remote device and endpoint, failure handling for the remote link, and a Nexus to Edge reference demo that runs in one process. PLANNED or PROPOSED only: Sense, Mind, Motion, Skill, Sim, SDK, SoC, drivers, real transports and real hardware, and the later phases described in [docs/architecture/KRITVA_VISION.md](docs/architecture/KRITVA_VISION.md); the top-level build adds those directories only if they exist, and none of them exists in this repository today. NOT YET IMPLEMENTED: any physical device, real network transport or real-time behaviour. Two human safety reviews (KOS-I3 and KOS-I4) are OPEN; see section 13. The planning records' words "Done", "IMPLEMENTED" and "SOFTWARE IMPLEMENTATION COMPLETE" are this repository's own statements; they are not central acceptance by kritva-program and do not mean safe, released or fit for any robot.

## 2. Position in the KritvaOS Ecosystem

This repository is the parent of the `core` submodule (`kritva-core`) and integrates it with the runtime and hardware layers. Its documented layering is in [ARCHITECTURE.md](ARCHITECTURE.md) and [docs/architecture/REPOSITORY_ARCHITECTURE.md](docs/architecture/REPOSITORY_ARCHITECTURE.md). The other Kritva repositories (sense, motion, mind, skill, raas and the actuator family) are separate repositories; whether and how they are attached here is decided in kritva-program, and none is attached today. The roster and order of repositories are owned by kritva-program (`v2/planning/repo-pipeline.json`), not by this README.

## 3. Architecture

[ARCHITECTURE.md](ARCHITECTURE.md) is the canonical system architecture and [docs/architecture/REPOSITORY_ARCHITECTURE.md](docs/architecture/REPOSITORY_ARCHITECTURE.md) says where functionality belongs. Component documents: [runtime](docs/architecture/runtime/RUNTIME_ARCHITECTURE.md), [device and endpoint](docs/architecture/hardware/DEVICE_ENDPOINT_ARCHITECTURE.md), and the KOS-I4 [protocol](docs/architecture/KOS-I4_PROTOCOL.md), [transport](docs/architecture/KOS-I4_TRANSPORT_ARCHITECTURE.md) and [remote architecture](docs/architecture/KOS-I4_REMOTE_ARCHITECTURE.md). The runtime and hardware layers are C++ libraries under `runtime/` and `hardware/`, built on the Core library. The architecture documents describe the intended system as well as what exists; read each document's own status and the implementation state in section 1.

## 4. Documents to Read First

Role first, then the status the document declares. A label is a role, not an approval; where a document does not declare a status in its header, none is shown here.

| # | Document | Label, status | What it answers |
|---|---|---|---|
| 1 | [AGENTS.md](AGENTS.md) | AUTHORITATIVE for agent and contributor rules; in force | How to work here: authority order, boundaries, submodule and governance rules |
| 2 | [ARCHITECTURE.md](ARCHITECTURE.md) | AUTHORITATIVE for the system architecture; status per the file | What the platform is meant to be and its boundaries |
| 3 | [docs/architecture/](docs/architecture/) | AUTHORITATIVE for the component architectures; status per file | Runtime, device, protocol, transport and remote architecture |
| 4 | [docs/requirements/](docs/requirements/) | AUTHORITATIVE for the requirements; status per file | What the runtime and device layers must do |
| 5 | [docs/verification/](docs/verification/) | AUTHORITATIVE for the verification and regression strategy; status per file | What is verified and how |
| 6 | [toolchain/VERSIONS.yaml](toolchain/VERSIONS.yaml) | AUTHORITATIVE for supported tool versions (pre-alpha definition) | Which tools and versions are supported |
| 7 | [planning/README.md](planning/README.md) | AUTHORITATIVE for planning and execution control | How milestones and tasks are run |
| 8 | [planning/MILESTONE_STATUS.md](planning/MILESTONE_STATUS.md) | REFERENCE: a consolidated view of milestone status and closure gates | Where each milestone and its open reviews stand |
| 9 | [CONTRIBUTING.md](CONTRIBUTING.md) and [CLA.md](CLA.md) | AUTHORITATIVE for contribution terms | Licence, CLA, contribution and review rules |
| 10 | [SECURITY.md](SECURITY.md) | AUTHORITATIVE for vulnerability reporting (route tested 18 October 2026, owner-attested) | How to report a vulnerability |
| 11 | [status/README.md](status/README.md) | REFERENCE | How the status report is generated and what it does not mean |
| 12 | [docs/architecture/KRITVA_VISION.md](docs/architecture/KRITVA_VISION.md) | REFERENCE: vision and target narrative, not implemented state | The long-term intent that the old README described |
| 13 | [CHANGELOG.md](CHANGELOG.md) | REFERENCE | What changed |

## 5. Repository Structure and File Responsibilities

Labels: INPUT is edited by hand and read by a tool or review; GENERATED is written by a tool and never edited; EVIDENCE supports a claim; AUTHORITATIVE owns a rule; REFERENCE governs nothing. A label is a role, not an approval.

| Path | Content | Label, status | Notes |
|---|---|---|---|
| [core/](core/) | Git submodule `kritva-core` (pointer recorded in the repository; checked out only with `--recurse-submodules` or `make submodules`) | INPUT; IMPLEMENTED upstream | This repository never changes Core; the pointer is changed only by an explicit decision |
| [runtime/](runtime/) | KOS-I2 application runtime host, configuration and observation | INPUT; IMPLEMENTED (host software) | |
| [hardware/](hardware/) | `abstraction/`, `mock/`, `transport/` and `remote/` (KOS-I3 and KOS-I4) | INPUT; IMPLEMENTED (host software, mock devices and simulated transport only) | No real hardware driver or physical transport |
| [examples/](examples/) | `kritva_demo`, `device_demo` and `nexus_edge_demo` | INPUT; IMPLEMENTED | Demos run in one process; they are not hardware demonstrations |
| [tests/](tests/) | Runtime, hardware, source-header and commit-message tests | EVIDENCE; IMPLEMENTED | Host tests only |
| [docs/](docs/) | Architecture, requirements, verification and development documents | MIXED: AUTHORITATIVE and REFERENCE per document | |
| [planning/](planning/) | Milestone plans, tasks, acceptance criteria, review checklists, change logs | MIXED: planning records and views | Governs execution and acceptance of milestones |
| [project/](project/) | Roadmap, release checklist and matrix, tracking registers | REFERENCE; status per file | May lag the planning records |
| [scripts/](scripts/) | Toolchain check, status generator, source-header lint, commit checkers | INPUT; IMPLEMENTED | |
| [status/](status/) | Status report, schema, curated evidence file | GENERATED (`status.json`); INPUT (`evidence.json`, owner-curated) | Never hand-edit `status.json` |
| [toolchain/](toolchain/), [config/](config/), [.devcontainer/](.devcontainer/), [.githooks/](.githooks/) | Toolchain definition, header-lint configuration, development container, Git hooks | INPUT | |
| [CMakeLists.txt](CMakeLists.txt), [CMakePresets.json](CMakePresets.json), [Makefile](Makefile) | Build, presets and local commands | INPUT; IMPLEMENTED | `make help` lists the commands |
| `sense/`, `mind/`, `motion/`, `skill/`, `sim/`, `sdk/`, `drivers/`, `soc/`, `tools/` | Directories the build would add if present | NOT YET IMPLEMENTED (absent) | Named in [docs/architecture/REPOSITORY_ARCHITECTURE.md](docs/architecture/REPOSITORY_ARCHITECTURE.md) as intended |
| [FOLDER.md](FOLDER.md), [ownership.md](ownership.md), [responsibilities.md](responsibilities.md), [document.md](document.md), [plan.md](plan.md) | Early structure and planning notes | REFERENCE; may be stale | [docs/development/REPOSITORY_STRUCTURE.md](docs/development/REPOSITORY_STRUCTURE.md) is the structure document |
| `.github/` | CODEOWNERS, pull request template, workflows, agent instructions | AUTHORITATIVE | |

## 6. Toolchain and Development Workflow

| Tool | Purpose | Version | Check |
|---|---|---|---|
| CMake | Configure, build, test | 3.24 or later (the project requires C++20) | `cmake --version` |
| A C++20 compiler | Build | see [toolchain/VERSIONS.yaml](toolchain/VERSIONS.yaml); CI builds inside the development container | `c++ --version` |
| Python 3 | Header lint, status, toolchain and commit scripts | see the toolchain definition | `python3 --version` |
| Docker or a dev container | The CI environment | [.devcontainer/Dockerfile](.devcontainer/Dockerfile) | `docker --version` |
| GNU make | Convenience wrapper | any | `make --version` |

The toolchain definition is [toolchain/VERSIONS.yaml](toolchain/VERSIONS.yaml); `python3 scripts/check_toolchain.py --profile ci --strict` checks an environment against it. Development follows [AGENTS.md](AGENTS.md): read the architecture and requirements first, keep to the owning repository's boundaries, and do not change the Core pointer unless explicitly asked.

```bash
git clone --recurse-submodules https://github.com/KritvaOS/kritvaos-community   # modify: creates a local working copy
cd kritvaos-community
cmake --preset debug                                                           # generate: writes build configuration under build/debug
cmake --build build/debug                                                      # modify: writes build outputs under build/debug
ctest --test-dir build/debug --output-on-failure                               # check: runs the host test suite
```

If the clone was made without submodules, run `make submodules` (or `git submodule update --init`) first; the build needs the `core` submodule. The other presets are `release`, `asan` and `ubsan`; see [CMakePresets.json](CMakePresets.json).

## 7. Dependencies and Interfaces

| Dependency | Direction | Kind | Contract | State |
|---|---|---|---|---|
| kritva-core | upstream | Git submodule (`core`) and CMake source dependency | The public API and compatibility contract of Core | Configured in `.gitmodules`; a pointer is recorded (this README does not name the commit and does not claim the pinned commit was independently verified); CI checks the submodule out recursively and builds against it |
| Platform contracts of Core | upstream | Core defines the platform contracts; this repository supplies the runtime and mock and simulated implementations | `core/include/kritva/core/platform/` | Contracts exist upstream; only simulated and mock implementations exist here; no real platform adapter is verified here |
| Third-party libraries | upstream | see [toolchain/VERSIONS.yaml](toolchain/VERSIONS.yaml) | The toolchain definition | Not listed individually here |
| kritva-program | upstream | governance and status | status schema 2.0 ([status/README.md](status/README.md)) | planning-only reporting; no program credit |
| kritva-actuator family and other Kritva repositories | peers or downstream | none attached | none | PLANNED or PROPOSED in the program's repository plan; no source or runtime link exists here |

Five kinds of relationship are kept apart: program ordering, Git submodule, source or build dependency, runtime integration, and governance or status reporting. A PROPOSED or PLANNED dependency is not an integration that exists.

## 8. Milestones and Implementation Plan

Work is organised in milestones, each with a folder under [planning/milestones/](planning/milestones/). The milestone IDs and their repository-declared status, from [planning/MILESTONE_STATUS.md](planning/MILESTONE_STATUS.md):

| Milestone | Subject | Repository-declared status |
|---|---|---|
| KOS-I2 | Application runtime (host, composition, configuration, observation, failure handling, reference demo) | IMPLEMENTED; predecessor baseline |
| KOS-I3 | Device and endpoint integration | IMPLEMENTED; **human safety review OPEN** |
| KOS-I4 | Remote layer: protocol, transport, Edge host, remote endpoint, distributed failure handling, Nexus to Edge demo | Software implementation complete; **human safety review OPEN**; KOS-CI-001 (Release, ASan and UBSan in CI) deferred |

The current milestone pointer is [planning/README.md](planning/README.md), which still names KOS-I3 as current and may lag [planning/MILESTONE_STATUS.md](planning/MILESTONE_STATUS.md); treat the status document as the later record and check both. Those are this repository's own planning records: central acceptance and baseline membership are decided in kritva-program, and dates are planning information, not delivery commitments. [project/roadmap/ROADMAP.md](project/roadmap/ROADMAP.md) holds the longer roadmap and is a plan, not a commitment.

## 9. Tasks and Acceptance Criteria

- **Where tasks live.** The `TASKS.md` of each milestone under [planning/milestones/](planning/milestones/) (task IDs I2-xxx, I3-xxx and I4-xxx; the count is not copied here because it goes stale); [planning/TASKS.md](planning/TASKS.md) is the general list.
- **Acceptance.** Each milestone folder holds its `ACCEPTANCE_CRITERIA.md`, `REVIEW_CHECKLIST.md` and test plan. The safety gates are listed separately and left unchecked until a human signs them off.
- Work is not accepted because code compiles, a commit or merged pull request exists, a test passes locally or a milestone says "Done". Public API changes, boundary changes, safety and security behaviour, real-time guarantees and major dependency changes need human review ([AGENTS.md](AGENTS.md)).

## 10. Status Reporting and Program Integration

`status/status.json` (schema 2.0, workstream COMMUNITY) is generated by `scripts/gen_status.py`; never edit it by hand. It reports the tasks listed in those milestone files (no count is copied here). This repository reports facts only and earns no program credit; credit comes only from the baseline, approvals and evidence recorded in kritva-program ([status/README.md](status/README.md)).

| Dimension | Source record | Authority |
|---|---|---|
| Execution | the milestone `TASKS.md` files, reported in `status/status.json` (`COMPLETE` means the task's status says Done) | The task owner records it |
| Evidence | each task's `ACCEPTANCE_CRITERIA.md` (a criteria reference, never proof) and the owner-curated [status/evidence.json](status/evidence.json) (commit-bound CI runs) | The author records it; a reviewer other than the author checks it |
| Acceptance | the milestone acceptance criteria, the architect review, and the central approvals in kritva-program | The independent reviewer and the owner, never a status report |
| Release readiness | the release checklist ([project/releases/RELEASE_CHECKLIST.md](project/releases/RELEASE_CHECKLIST.md)) and the open safety gates | The owner, through the central gates |

A run recorded in the evidence file is a fact about a commit, not acceptance. A generated view is a snapshot that can lag its inputs, cannot approve its own work and does not establish program credit; `readiness` in the report is a fixed default and is not safety clearance.

## 11. Build, Test, Regression and Validation

CI runs six workflows. `make check` is a local command and is not the same as CI.

| Workflow | Trigger | What it runs |
|---|---|---|
| `KritvaOS CI` (`ci.yml`) | push and pull request on `main`, `master`, `develop` | Toolchain validation in the development container; a Debug build and `ctest`; a matrix of the `release`, `asan` and `ubsan` presets (build and `ctest`); the source-header check |
| `Source Header Check` | every push and pull request | `python3 scripts/lint/check_source_headers.py --mode tracked --strict` |
| `Status Report Check` | every push and pull request | the status generator tests, `gen_status.py --check-facts` and the schema checksum |
| `commit-message-check` | pull requests | the unit tests of both commit checkers, then `check_commit_messages.py` on the commits in the pull request (rejects session trailers and links) and `check_commit_format.py` on the same commits (the D-053 form; blocking since D-061) |
| `readme-check` | pull requests; push to `main` | the README checker's unit tests, then `python3 scripts/check_readme.py README.md` (structure, metadata and local links only; not a required check yet, D-067) |
| `secret-scan` | push; pull requests; manual dispatch | gitleaks |

The planning record says Release, ASan and UBSan results were local evidence only and that KOS-CI-001 (Release, ASan and UBSan in CI) is deferred. `ci.yml` now defines that matrix, but a workflow definition is not a passing run and not an accepted gate: whether the jobs pass is reported by CI, and whether KOS-CI-001 is closed is a planning-record and owner decision. Until the planning record is updated, treat KOS-CI-001 as deferred.

| Command | Effect | Notes |
|---|---|---|
| `make help`, `make info`, `make docs`, `make tree`, `make environment`, `make status`, `make branch`, `make submodule-status`, `make git-hooks`, `make header-check-version` | read-only | information only |
| `make header-check` | check | source-header lint |
| `make check` | check | runs `header-check` only; it does not build or test |
| `make check-readme` | check | structural check of this README only (metadata, topics, relative links, placeholders); a PASS does not show the content is accurate; not part of `make check` or CI |
| `make pre-pr` | check | runs `make check` and `make status` |
| `make submodules` | modify (fetches the `core` submodule) | needs network |
| `make clean` | modify (deletes local generated artefacts) | local only |

There is no `make build` or `make test`: build and test use the CMake presets shown in section 6. A passing `make check` confirms only that the source-header check passed: it does not build or test the code, run the status checks or run any other CI workflow. Passing CI shows that the workflows' own commands passed on that commit. It does not show that the design is right, that a gate is closed, that the runtime meets a real-time requirement or that a robot using it is safe. No hardware, real-time or physical test exists here.

## 12. Git Workflow and Review

Controlling statement: [AGENTS.md](AGENTS.md), section 48, "Program Governance Rules" gives the D-053 form: `type(scope): summary` with one `Task:` or `Tasks:` footer, small logical commits and a merge-commit merge. [CONTRIBUTING.md](CONTRIBUTING.md) section 20 gives the same rule (D-062). The owner adopted the Kritva commit standard in this repository (D-061, the community part of PD-019): the `commit-message-check` workflow runs `scripts/security/check_commit_format.py` on every commit in a pull request, together with the `Claude-Session:` trailer and session-link check. Work on a branch and open a pull request; do not push to `main`, force-push a shared branch or rewrite history without the owner's explicit approval. `main` is protected by a ruleset that requires `secret-scan`, `commit-message-check` and Code Owner review (owner statement, D-040; not independently verified); with one owner the PR author cannot self-approve, so use the owner bypass or a second reviewer. Submodule changes are committed in the submodule's own repository, and this repository's pointer is not changed unless explicitly requested.

## 13. Safety and Security

This repository contains software for robotic devices, including an actuator write path (motor commands, limits, a heartbeat-timeout stop and fault-to-safe-state behaviour) that is exercised only against mock devices and a simulated transport. It is not a safety-certified component and no safety case is made for it. Two human safety reviews are OPEN and are recorded as such in [planning/MILESTONE_STATUS.md](planning/MILESTONE_STATUS.md): the KOS-I3 review (`MotorCommand`, motor limits, fault-to-zero behaviour, velocity command semantics, mock safety behaviour, any future mapping to physical actuator control) and the KOS-I4 review (the actuator write paths: Edge limit validation, duplicate and replay protection, heartbeat-timeout stop, fault-to-safe-state). **Connecting this software to a physical actuator is blocked while these gates are open.** Software tests, mock devices and simulation do not establish physical safety, and nothing here should be called validated or safe on that evidence. The open human safety reviews are tracked centrally in kritva-program, not only here.

**Reporting a vulnerability:** follow [SECURITY.md](SECURITY.md). It names GitHub private vulnerability reporting as the first route and gives a direct-contact fallback. The Security Owner reports having tested the reporting route on 18 October 2026 (D-068); that is ATTESTED, not independently verified, and nothing here promises a response time. Never put details of an unfixed vulnerability in a public issue, pull request or commit message.

- Never commit secrets, private keys, tokens, passwords or production data. Use obviously fake values. The `secret-scan` workflow runs on every push and pull request; do not add allowlist entries to silence it.
- If a secret is committed by mistake, tell the Security Owner at once; removing it in a later commit does not make it safe.

## 14. Release and Versioning

The build version is set in [CMakeLists.txt](CMakeLists.txt) (0.1.0) and the release notes are in [CHANGELOG.md](CHANGELOG.md). The release checklist and matrix are in [project/releases/](project/releases/). The repository is pre-alpha: APIs, module boundaries and interfaces may change significantly, and no release is recorded by this README. A release is complete only when the release gate and the open safety gates are met, not when code is merged.

## 15. Ownership and Escalation

| Role | Responsibility |
|---|---|
| Repository owner (Dinesh; `.github/CODEOWNERS`) | Directs the work, decides, approves and merges |
| Security Owner | Handles vulnerability reports and committed secrets |
| Independent reviewer (architect) | Reviews and accepts tasks against the milestone acceptance criteria |
| Human safety reviewer | Signs off the open KOS-I3 and KOS-I4 safety reviews |
| Implementers (people and agents) | Implement approved scope, write tests, record evidence and follow `AGENTS.md` |

## 16. Related Repositories and References

| Repository | Relationship |
|---|---|
| [kritva-program](https://github.com/KritvaOS/kritva-program) | Program plan, baseline, decisions, approvals, status consolidation, README standard |
| [kritva-core](https://github.com/KritvaOS/kritva-core) | The `core` submodule: the platform-independent runtime foundation library |

## 17. License

Apache License 2.0 for the code in this repository. Copyright (c) 2026 KritvaOS. See [LICENSE](LICENSE) and [CONTRIBUTING.md](CONTRIBUTING.md) for contribution and licensing terms. The owner confirmed Apache-2.0 as the licence of this repository (program decision D-064). The documents say the same without the word "permanently": [.github/README.md](.github/README.md) and [CONTRIBUTING.md](CONTRIBUTING.md) state the Apache-2.0 licence, and [docs/architecture/KRITVA_VISION.md](docs/architecture/KRITVA_VISION.md) (REFERENCE) says that future components outside this repository may be licensed differently. That is a statement of intent, not a licence term; only the licence file governs.
