> **Status of this document: REFERENCE; vision and target architecture narrative.** It was the repository README until the Kritva README standard was adopted and is kept here unchanged below this note. It describes what Kritva is intended to become. It does not say what is implemented, accepted, safe or released, and statements in it such as the roadmap phases, target platforms, repository tree, the GitHub address and licensing intentions are not approvals. For the implemented state read the [README](../../README.md), [ARCHITECTURE.md](../../ARCHITECTURE.md) and the planning and status records.

---

# KRITVA

### Open Robotic Computing Platform

**Kritva connects Physical AI to the physical world.**

Kritva is an open, hardware-aware **robotic computing platform** designed to connect:

- Physical AI
- Robotics software
- Real-time computing
- Distributed control
- Heterogeneous compute
- Sensors and actuators
- Robotic hardware
- Simulation and digital twins
- FPGA and silicon implementations
- Developer applications

Kritva is built around a simple idea:

> **A robot is not just a software application running on a computer.  
> A robot is a distributed computing system interacting with the physical world in real time.**

Kritva aims to provide a continuous computing architecture from:

```text
Physical AI
     ↓
Robotics Software
     ↓
Real-Time Compute
     ↓
Distributed Control
     ↓
Robotic Hardware
     ↓
Physical World
```

Kritva is intended to provide a common architecture spanning intelligence, software, compute, control, hardware, and physical endpoints.

---

# Vision

Modern robots combine AI, perception, motion control, embedded computing, networking, sensors, actuators, and increasingly specialized hardware.

However, these layers are often developed as separate systems.

Kritva aims to provide a common architecture that connects them.

```text
             Physical AI
                  │
                  ▼
          ┌───────────────┐
          │ Kritva Mind   │
          │ Intelligence  │
          └───────┬───────┘
                  │
                  ▼
          ┌───────────────┐
          │ Kritva Skill  │
          │ Capabilities  │
          └───────┬───────┘
                  │
          ┌───────┴────────┐
          ▼                ▼
   ┌────────────┐   ┌────────────┐
   │ Kritva     │   │ Kritva     │
   │ Sense      │   │ Motion     │
   │ Perception │   │ Control    │
   └─────┬──────┘   └─────┬──────┘
         │                │
         └────────┬───────┘
                  ▼
          ┌───────────────┐
          │ Kritva Core   │
          │ Runtime       │
          └───────┬───────┘
                  │
                  ▼
        Hardware Abstraction
                  │
          ┌───────┴────────┐
          ▼                ▼
   Kritva Nexus       Kritva Edge
   System Compute     Distributed Compute
          │                │
          └───────┬────────┘
                  ▼
              Subnodes
                  │
                  ▼
              Endpoints
                  │
                  ▼
         Sensors / Actuators
                  │
                  ▼
          Physical World
```

The long-term objective is:

> **From Physical AI to Real-World Robots.**

---

# What is Kritva?

Kritva is organized as a robotic computing platform rather than as a single operating system, robot, or processor.

```text
Kritva
│
├── KritvaOS
│   ├── Core
│   ├── Sense
│   ├── Mind
│   ├── Motion
│   ├── Skill
│   ├── Sim
│   └── SDK
│
└── Kritva Hardware Platform
    ├── Nexus
    ├── Edge
    ├── Subnode
    └── Endpoint
```

This architecture allows Kritva software, compute platforms, hardware interfaces, and future silicon implementations to evolve independently while maintaining common interfaces.

---

# Kritva Computing Stack

The Kritva computing model is:

```text
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
Kritva Nexus
     ↓
Kritva Edge
     ↓
Subnode
     ↓
Endpoint
     ↓
Physical World
```

The architecture is intentionally layered.

Each layer has a defined responsibility while remaining connected to the layers above and below it.

---

# From Physical AI to Physical Action

A robot must convert intelligence into physical action.

Kritva provides a conceptual path:

```text
AI Model
   ↓
World Understanding
   ↓
Reasoning
   ↓
Planning
   ↓
Skill
   ↓
Motion Intent
   ↓
Real-Time Control
   ↓
Distributed Compute
   ↓
Motor / Actuator
   ↓
Physical Action
```

At the same time, information flows back from the physical world:

```text
Physical World
      ↓
Sensors
      ↓
Endpoints
      ↓
Edge Compute
      ↓
Nexus Compute
      ↓
Perception
      ↓
World Model
      ↓
Reasoning
```

This creates a continuous loop:

```text
Perceive
   ↓
Understand
   ↓
Decide
   ↓
Act
   ↓
Observe
   ↓
Learn
   ↺
```

---

# KritvaOS

**KritvaOS** is the open robotic software platform within Kritva.

It provides the software foundation required to build intelligent and embodied robotic systems.

KritvaOS is organized into modular subsystems:

```text
KritvaOS
│
├── Kritva Core
├── Kritva Sense
├── Kritva Mind
├── Kritva Motion
├── Kritva Skill
├── Kritva Sim
└── Kritva SDK
```

The architecture is designed to support both centralized and distributed robotic computing.

---

# Kritva Core

**Kritva Core** provides the common software foundation shared across the Kritva platform.

Core is intended to provide platform-independent primitives and runtime foundations rather than becoming a container for every hardware or robotics function.

Core responsibilities include foundational services such as:

- Identity
- Lifecycle
- Status
- Health
- Statistics
- Error and fault handling
- Result types
- Events
- Capabilities
- Configuration
- Versioning
- Time primitives
- Metadata
- Runtime foundations
- IPC abstractions
- Resource management
- Scheduling foundations
- Diagnostics foundations

The architecture separates these common primitives from hardware-specific implementations.

```text
Kritva Core
│
├── Common Platform Primitives
│   ├── Identity
│   ├── Lifecycle
│   ├── Status
│   ├── Health
│   ├── Statistics
│   ├── Errors
│   ├── Events
│   ├── Capability
│   ├── Configuration
│   ├── Time
│   └── Metadata
│
└── Runtime / Platform Services
        │
        ▼
Hardware Abstraction
        │
        ├── Drivers
        ├── Nexus
        ├── Edge
        └── Endpoint
```

Kritva Core is intended to remain independent of:

- A specific robot
- A specific CPU vendor
- A specific GPU
- A specific network technology
- A specific AI framework
- A specific database
- A specific cloud service

---

# Kritva Sense

**Kritva Sense** is the perception and sensor framework.

Potential capabilities include:

- Camera interfaces
- IMU
- LiDAR
- Depth sensors
- Force/torque sensors
- Joint sensors
- Audio
- Sensor synchronization
- Sensor fusion
- Object detection
- Human detection
- Tracking
- Environment perception
- State estimation

The objective is to transform raw sensor information into structured information usable by the rest of the robotic system.

```text
Sensors
   ↓
Sensor Drivers
   ↓
Kritva Sense
   ↓
Sensor Processing
   ↓
Fusion / Perception
   ↓
World Representation
```

---

# Kritva Mind

**Kritva Mind** is the intelligence and reasoning layer.

Potential capabilities include:

- AI inference
- World models
- Reasoning
- Planning
- Decision making
- Task decomposition
- Context management
- Natural-language interaction
- Cognitive memory
- Behavior planning
- AI model integration

Kritva Mind connects perception with purposeful action.

```text
Kritva Sense
      ↓
World Understanding
      ↓
Kritva Mind
      ↓
Reasoning
      ↓
Planning
      ↓
Task / Skill
```

Kritva does not require a single AI model or AI vendor.

The architecture is intended to allow models and AI technologies to evolve independently from the underlying robotic computing platform.

---

# Kritva Motion

**Kritva Motion** is the physical control layer.

Potential capabilities include:

- Motor control
- Joint control
- Whole-body control
- Locomotion
- Balance
- Walking
- Running
- Manipulation
- Trajectory generation
- Inverse kinematics
- Dynamics
- Motion planning
- Safety limits
- Real-time control interfaces

The objective is to convert high-level intent into safe, coordinated physical movement.

```text
Skill / Planning
      ↓
Motion Intent
      ↓
Motion Planning
      ↓
Whole-Body / Joint Control
      ↓
Real-Time Control
      ↓
Edge Compute
      ↓
Actuator
```

---

# Kritva Skill

**Kritva Skill** provides reusable robot capabilities and behaviors.

A Skill represents something the robot knows how to do.

Examples:

```text
Stand()
Sit()
Walk()
Reach()
Grasp()
Release()
PickObject()
OpenDoor()
FollowPerson()
NavigateTo()
Speak()
LookAt()
```

Skills can compose multiple Kritva subsystems.

Example:

```text
PickObject()
     │
     ├── Sense  → DetectObject
     ├── Mind   → SelectObject
     ├── Motion → MoveArm
     ├── Motion → Grasp
     └── Sense  → VerifyGrasp
```

This provides a foundation for composable and reusable robot capabilities.

---

# Kritva Sim

**Kritva Sim** is the simulation, validation, and digital-twin environment.

Potential capabilities include:

- Robot simulation
- Digital twins
- Physics simulation
- Sensor simulation
- Motion validation
- AI testing
- Software-in-the-loop (SIL)
- Hardware-in-the-loop (HIL)
- Regression testing
- Scenario generation
- Failure testing
- Hardware/software validation

The long-term development objective is:

> **Develop in simulation → Validate → Deploy to the physical robot**

Kritva Sim should enable the same interfaces to be exercised across:

```text
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
```

---

# Kritva SDK

**Kritva SDK** is the developer platform for building applications and extending Kritva.

Potential capabilities include:

- Application APIs
- SDK libraries
- Robot interfaces
- Skill development
- Simulation interfaces
- AI interfaces
- Hardware interfaces
- Debugging tools
- CLI tools
- Development examples
- Documentation

Illustrative future API:

```python
from kritva import Robot

robot = Robot()

robot.connect()

robot.skills.stand()
robot.skills.walk_to("kitchen")
robot.skills.pick("bottle")
robot.skills.return_to("table")
```

> **Note:** APIs shown above are illustrative and are not currently frozen.

---

# Kritva Robotic Computing

Kritva extends beyond software.

The robotic computing architecture provides a distributed compute hierarchy:

```text
Kritva Robotic System
│
├── System Compute
│       └── Nexus
│
├── Distributed Real-Time Compute
│       └── Edge
│
├── Local Device Compute
│       └── Subnode
│
└── Physical Interface
        └── Endpoint
```

This architecture allows compute to be placed where it is most appropriate.

High-level intelligence does not need to execute in the same location as time-critical motor control.

---

# Kritva Nexus

**Kritva Nexus** is the system-level compute platform within the Kritva hardware architecture.

Nexus is intended for workloads such as:

- High-performance CPU processing
- AI inference
- Vision workloads
- World models
- Planning
- System coordination
- Global robot state
- Storage
- Security
- Networking
- System management
- EtherCAT connectivity
- Developer and application workloads

A typical Nexus environment may use:

- Linux
- PREEMPT_RT where required
- CPU compute
- GPU
- NPU
- Vision accelerators
- Other specialized accelerators

Nexus is a **platform family**, not necessarily one fixed physical chip.

Different robotic workloads may require different Nexus implementations.

---

# Kritva Edge

**Kritva Edge** is the distributed real-time compute platform.

Edge is intended for workloads such as:

- Motor control
- Joint control
- Encoder processing
- Real-time I/O
- PWM
- ADC
- GPIO
- SPI
- I2C
- UART
- EtherCAT
- Local diagnostics
- Safety-related local control

A typical Edge platform may use:

- Real-time CPU
- MCU-class processing
- Real-time peripherals
- EtherCAT interface
- Local hardware acceleration
- Dedicated control logic

Edge platforms can be distributed throughout the robot.

For example:

```text
                 Nexus
                   │
             EtherCAT / Network
                   │
       ┌───────────┼───────────┐
       ▼           ▼           ▼
    Edge        Edge        Edge
   Arm/Body     Leg         Head
       │           │           │
    Subnodes    Subnodes    Subnodes
       │           │           │
   Endpoints   Endpoints   Endpoints
```

---

# One Architecture, Multiple Robots

Kritva is not limited to one robot type.

The common architecture is:

```text
Application
     ↓
Skill
     ↓
Mind / Motion
     ↓
Sense
     ↓
Core
     ↓
Nexus
     ↓
Edge
     ↓
Subnode
     ↓
Endpoint
```

The implementation of each layer can be specialized for the robot.

## Humanoid Robots

```text
Nexus
  │
  ├── Edge → Arm
  ├── Edge → Arm
  ├── Edge → Leg
  ├── Edge → Leg
  ├── Edge → Head
  └── Edge → Body
```

Humanoids may require high sensor density, distributed motion control, whole-body coordination, and significant AI compute.

## Manipulators and Cobots

```text
Nexus
  │
  ├── Edge → Joint Group
  ├── Edge → End Effector
  └── Edge → Sensor System
```

Focus areas include:

- Precision motion
- Force control
- Manipulation
- Vision
- Safety

## Mobile Robots

```text
Nexus
  │
  ├── Edge → Drive
  ├── Edge → Sensors
  └── Edge → Actuators
```

Focus areas include:

- Navigation
- Perception
- Localization
- Drive control

## Quadruped Robots

```text
Nexus
  │
  ├── Edge → Front Left
  ├── Edge → Front Right
  ├── Edge → Rear Left
  └── Edge → Rear Right
```

Focus areas include:

- Balance
- Leg control
- Terrain perception
- Locomotion

## Aerial Robots

Aerial systems may use specialized compute optimized for:

- Weight
- Power
- Flight control
- Sensor fusion
- Navigation
- AI inference

The physical implementation may differ while maintaining the common Kritva architectural model.

---

# Common Architecture, Multiple Compute Implementations

Kritva does not assume that one processor architecture, one SoC, or one hardware vendor will fit every robot.

The platform is designed to support:

- ARM
- RISC-V
- x86
- GPU platforms
- AI accelerators
- FPGA
- Custom SoCs
- MCU/RTOS systems
- Linux systems
- Heterogeneous compute

The goal is:

> **One common Kritva architecture with a family of compute implementations optimized for different robotic workloads.**

---

# Hardware Abstraction

Kritva separates software interfaces from physical hardware implementations.

```text
Kritva Software
       │
       ▼
Hardware Abstraction
       │
       ├── CPU
       ├── GPU
       ├── NPU
       ├── FPGA
       ├── Sensor
       ├── Actuator
       ├── Network
       └── Storage
```

This allows the same higher-level software architecture to operate across different hardware platforms.

Hardware-specific functionality should remain below clearly defined interfaces.

---

# Distributed Communication

Robotic systems require communication across multiple compute domains.

Kritva is designed to support communication between:

```text
Application
     ↕
Nexus
     ↕
Edge
     ↕
Subnode
     ↕
Endpoint
```

The architecture should support appropriate communication technologies depending on the workload.

Examples include:

- Ethernet
- EtherCAT
- PCIe
- SPI
- I2C
- UART
- GPIO
- Other platform-specific interfaces

No single transport is assumed to be appropriate for every layer.

---

# Real-Time Architecture

Not every robotic workload requires hard real-time behavior.

Kritva therefore separates:

```text
High-Level Compute
    │
    ├── AI
    ├── Reasoning
    ├── Planning
    └── Applications
            │
            ▼
Real-Time Control
    │
    ├── Motion
    ├── Joint Control
    ├── Motor Control
    └── Safety-Critical Paths
```

Time-critical paths should use deterministic execution where required.

Possible implementation environments include:

- Linux
- PREEMPT_RT
- RTOS
- MCU firmware
- FPGA logic
- Dedicated hardware control

The architecture should avoid forcing all workloads into the same timing model.

---

# ROS2 Integration

Kritva is not intended to unnecessarily replace existing robotics ecosystems.

ROS2 can provide important capabilities for:

- Robotics middleware
- Communication
- Tooling
- Visualization
- Ecosystem integration
- Research
- Application development

Kritva can integrate with ROS2 while providing a broader robotic computing architecture.

Conceptually:

```text
Kritva
   │
   ├── KritvaOS
   │
   ├── Kritva Hardware
   │
   └── ROS2 Integration
```

Where appropriate, Kritva may reuse established technologies such as:

- ROS2
- `ros2_control`
- DDS
- micro-ROS

The goal is interoperability rather than unnecessary reinvention.

---

# Hardware and Software Co-Design

Robotic workloads increasingly require hardware and software to be designed together.

Kritva therefore considers:

```text
AI Workload
     ↓
Software Architecture
     ↓
Compute Requirements
     ↓
Hardware Architecture
     ↓
Accelerators
     ↓
FPGA / Silicon
```

This allows future Kritva implementations to optimize:

- AI inference
- Vision
- Sensor processing
- Real-time control
- Motion computation
- Networking
- Security
- Power efficiency

---

# Simulation → FPGA → Silicon → Robot

Kritva is intended to support a progressive validation path.

```text
Requirements
      ↓
Architecture
      ↓
API Definition
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
Physical Robot
```

The objective is to reduce development risk by validating the architecture before committing to physical hardware.

The same interfaces should be exercised as consistently as practical across these environments.

---

# Safety

Safety is an architectural concern.

Kritva should provide mechanisms and boundaries for:

- Operational limits
- Fault detection
- Fault reporting
- State monitoring
- Emergency behavior
- Motion limits
- Actuator limits
- Communication failure
- Sensor failure
- Compute failure
- Watchdogs
- Recovery

Safety-critical functionality should remain clearly separated from non-critical AI and application workloads.

---

# Security

Kritva is designed with security as a system-level concern.

Potential areas include:

- Device identity
- Secure boot
- Software integrity
- Authentication
- Authorization
- Secure communication
- Key management
- Access control
- Secure updates
- OTA security
- Debug access control
- Runtime protection
- Hardware security

Security mechanisms should be integrated into the architecture rather than added only after implementation.

---

# Observability

Robots operate in physical environments where failures can be difficult to reproduce.

Kritva therefore emphasizes observability.

The platform should expose structured information such as:

```text
Identity
Lifecycle
Status
Health
Statistics
Events
Errors
Capabilities
Configuration
Timestamp
Diagnostics
```

This information should be usable for:

- Development
- Debugging
- Simulation
- Validation
- Production diagnostics
- Fleet monitoring
- Failure analysis

---

# Configuration

Kritva configuration should be:

- Typed
- Versioned
- Validated
- Discoverable
- Hardware-aware
- Environment-aware

Configuration should not be tied to one serialization format.

YAML, JSON, TOML, databases, or other formats may be used by higher-level tools without becoming requirements of the Core API.

---

# Time

Robotic systems depend heavily on time.

Kritva considers time at multiple levels:

```text
Application Time
      ↓
System Time
      ↓
Distributed Time
      ↓
Real-Time Control
      ↓
Hardware Timestamp
```

The Core abstraction should provide common timestamp and duration primitives.

Specific synchronization mechanisms can be implemented at the appropriate platform layer.

Examples include:

- Monotonic clocks
- Hardware timestamps
- Network time synchronization
- PTP
- EtherCAT distributed clocks

---

# Storage

Kritva does not require a mandatory centralized database or cloud architecture.

Storage may be provided where required for:

- Robot configuration
- Logs
- Diagnostics
- Models
- Calibration
- Maps
- Mission data
- Local telemetry
- Historical information

Storage implementations remain deployment-specific.

---

# Customer APIs

Customer and application interfaces are first-class parts of the architecture.

Kritva should expose stable interfaces for:

- Robot state
- Sensors
- Motion
- Skills
- AI
- Configuration
- Diagnostics
- Simulation
- Hardware capabilities

The objective is to allow customers to build applications without depending unnecessarily on internal implementation details.

---

# Open Ecosystem

Kritva is intended to work with existing robotics, AI, compute, and hardware ecosystems.

Kritva does not need to replace every technology underneath it.

The architecture can integrate with:

- ROS2
- Linux
- PREEMPT_RT
- EtherCAT
- GPU platforms
- AI accelerators
- ARM
- RISC-V
- x86
- FPGA
- MCU/RTOS
- Existing sensors
- Existing motor controllers
- Existing robot hardware

The objective is to reduce fragmentation by providing a common architectural model.

> **Kritva does not need to replace every component of the robotic stack.  
> It needs to make the components work together through a common robotic computing architecture.**

---

# Design Principles

Kritva is being developed around the following principles.

## 1. Modular

Components should be independently replaceable and extensible.

## 2. Hardware Aware

Software should understand the capabilities and constraints of the underlying compute and physical system.

## 3. Hardware Agnostic

The architecture should support different processor, accelerator, and robot platforms.

## 4. Real-Time Where Required

Time-critical control paths should provide deterministic execution where required.

## 5. AI-Native

AI should be a first-class part of the robotic computing architecture.

## 6. Simulation First

Capabilities should be testable in simulation before deployment wherever practical.

## 7. Safety First

Robot actions must operate within defined physical and software safety boundaries.

## 8. Secure by Design

Identity, integrity, communication, access control, updates, and runtime security should be architectural concerns.

## 9. Developer Friendly

The platform should make it easy to build, test, debug, simulate, and deploy robotic applications.

## 10. Open Architecture

Interfaces should allow hardware, AI models, sensors, applications, and compute platforms to evolve independently.

## 11. Reuse Before Reinvention

Existing mature technologies should be integrated where they provide value.

## 12. Verify Before Scale

Architecture and interfaces should be validated before large-scale implementation.

---

# Architectural Boundaries

Kritva intentionally separates responsibilities.

```text
KritvaOS
│
├── Core
│   └── Common software foundations
│
├── Sense
│   └── Perception
│
├── Mind
│   └── Intelligence
│
├── Motion
│   └── Physical control
│
├── Skill
│   └── Reusable capabilities
│
├── Sim
│   └── Simulation and validation
│
└── SDK
    └── Developer interfaces
```

Hardware:

```text
Kritva Hardware Platform
│
├── Nexus
│   └── System compute
│
├── Edge
│   └── Distributed real-time compute
│
├── Subnode
│   └── Local device aggregation / processing
│
└── Endpoint
    └── Physical interface
```

This separation allows software and hardware to evolve independently.

---

# Repository Architecture

Kritva initially follows a modular repository architecture.

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
├── ARCHITECTURE.md
├── README.md
├── CONTRIBUTING.md
├── LICENSE
└── VERSION
```

Major reusable components may become independent repositories or Git submodules when there is a clear benefit in:

- Ownership
- Release lifecycle
- Reuse
- Interface stability
- Independent development
- External collaboration

The project will avoid unnecessary repository fragmentation during early development.

---

# Documentation Architecture

The root architecture document defines the system-level architecture.

```text
ARCHITECTURE.md
       │
       ▼
System Architecture
       │
       ├── Component Boundaries
       │
       ├── Compute Architecture
       │
       ├── Hardware Architecture
       │
       ├── Software Architecture
       │
       └── Verification Architecture
              │
              ▼
docs/architecture/
```

Important documentation areas include:

```text
docs/
├── architecture/
│   ├── REPOSITORY_ARCHITECTURE.md
│   ├── SOC_ARCHITECTURE.md
│   ├── FPGA_ARCHITECTURE.md
│   ├── RENODE_ARCHITECTURE.md
│   ├── DISTRIBUTED_COMPUTING.md
│   ├── REALTIME_ARCHITECTURE.md
│   ├── SAFETY_ARCHITECTURE.md
│   └── SECURITY_ARCHITECTURE.md
│
├── requirements/
│
├── api/
│
├── verification/
│
└── adr/
```

---

# Requirements and Traceability

Kritva development should maintain engineering traceability.

The intended development flow is:

```text
Requirement
    ↓
Architecture
    ↓
API
    ↓
Test Specification
    ↓
Implementation
    ↓
Unit Test
    ↓
Contract Test
    ↓
Integration Test
    ↓
System Validation
```

A requirement should be traceable to:

```text
Requirement
    ↓
File
    ↓
API
    ↓
Test
```

This becomes increasingly important as Kritva moves from experimentation toward production robotics.

---

# Verification Strategy

Kritva verification should operate at multiple levels.

```text
Unit
  ↓
Component
  ↓
Contract
  ↓
Integration
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
```

Potential verification environments include:

- Native host
- Cross-compiled targets
- Renode
- FPGA
- HIL
- Physical hardware
- Simulation environments

The objective is to detect architectural and implementation problems as early as possible.

---

# Development Methodology

Kritva follows a staged engineering methodology:

```text
Requirements
      ↓
Architecture
      ↓
API
      ↓
Test Specification
      ↓
Implementation
      ↓
Unit Tests
      ↓
Contract Tests
      ↓
Integration Tests
      ↓
Simulation
      ↓
Hardware Validation
      ↓
Review
      ↓
Release
```

AI-assisted development may be used for:

- Code generation
- Code review
- Test generation
- Documentation
- Static analysis
- Architecture review
- Verification assistance

Human engineering review remains responsible for architectural and safety decisions.

---

# First Vertical Slice

The most important early objective is not to build the entire robotic ecosystem.

It is to prove one complete path from software to physical action.

The first vertical slice should demonstrate:

```text
One Application
       ↓
One Skill
       ↓
One Mind Interface
       ↓
Kritva Core
       ↓
One Nexus
       ↓
One EtherCAT Edge
       ↓
One Motor + Encoder
       ↓
Physical Motion
```

The same path should eventually be validated through:

```text
Simulation
   ↓
Renode
   ↓
FPGA
   ↓
HIL
   ↓
Physical Robot
```

This vertical slice provides the foundation for scaling the architecture.

---

# Development Roadmap

## Phase 0 — Architecture Foundation

- Repository structure
- System architecture
- Repository architecture
- Coding standards
- Build system
- CI
- Documentation framework
- Requirements framework
- Verification framework
- Basic development environment

## Phase 1 — Kritva Core

- Core Foundation primitives
- Lifecycle
- Status
- Health
- Statistics
- Error handling
- Events
- Capability model
- Configuration model
- Time primitives
- Runtime foundations
- IPC foundations
- Diagnostics

## Phase 2 — Hardware Abstraction

- Hardware interface model
- Device model
- Driver interfaces
- Endpoint model
- Compute abstraction
- Nexus interface
- Edge interface

## Phase 3 — Kritva Sense

- Sensor abstraction
- Camera
- IMU
- Joint sensors
- Sensor synchronization
- Basic perception pipeline

## Phase 4 — Kritva Motion

- Joint control
- Motion primitives
- Robot model
- Kinematics
- Basic locomotion framework
- Distributed motion control

## Phase 5 — Kritva Mind

- AI runtime
- World model
- Planning
- Reasoning
- Task execution

## Phase 6 — Kritva Skill

- Skill model
- Skill registry
- Skill composition
- Behavior execution
- Application framework

## Phase 7 — Kritva Sim

- Simulation environment
- Digital twin
- SIL
- HIL
- Automated regression
- Scenario testing

## Phase 8 — Nexus / Edge Platforms

- Nexus architecture
- Edge architecture
- Renode models
- FPGA implementations
- Reference hardware
- HIL validation

## Phase 9 — Kritva SDK

- Developer APIs
- CLI
- Examples
- Documentation
- Application development framework

## Phase 10 — Robotic Platform

- Complete vertical slices
- Reference robotic systems
- Customer integrations
- Production validation
- Platform ecosystem

---

# Current Development Status

> 🚧 **Early Architecture / Pre-Alpha**

Kritva is currently in the architecture, experimentation, and technology-validation phase.

The project is focused on establishing:

- System architecture
- Software boundaries
- Hardware boundaries
- Core APIs
- Distributed computing architecture
- Nexus/Edge architecture
- Simulation strategy
- Verification methodology
- Development infrastructure

APIs, module boundaries, interfaces, and implementation details may change significantly during early development.

---

# Target Platforms

Kritva is intended to support a broad range of computing platforms, including:

- ARM-based systems
- RISC-V systems
- x86 development systems
- GPU-enabled systems
- Edge AI accelerators
- FPGA platforms
- Custom robotics SoCs
- MCU/RTOS systems
- Distributed robot controllers

The architecture aims to minimize unnecessary dependence on a specific processor, GPU, vendor, or robot manufacturer.

---

# Open Ecosystem

Kritva is intended to integrate with existing technologies rather than requiring a completely closed ecosystem.

Potential integration areas include:

```text
Operating Systems
    ├── Linux
    └── RTOS

Robotics
    ├── ROS2
    ├── ros2_control
    └── micro-ROS

Networking
    ├── Ethernet
    └── EtherCAT

Compute
    ├── ARM
    ├── RISC-V
    ├── x86
    ├── GPU
    ├── NPU
    └── FPGA

Hardware
    ├── Sensors
    ├── Motors
    ├── Encoders
    └── Actuators
```

---

# Licensing

The current KritvaOS public software repository is released under the **Apache License 2.0**.

See the [`LICENSE`](LICENSE) file for the complete license text.

The Apache License 2.0 permits use, modification, distribution, and integration of the software subject to its terms and conditions.

## Future Commercialization

Kritva is currently being developed as a public project during the early architecture, experimentation, and technology-validation phase.

The current Apache-2.0 license applies to code explicitly released under it. Owner decision D-064 confirms Apache-2.0 as the licence of the kritvaos-community repository; the statement below about future components concerns components outside it.

As the platform evolves, future components, hardware integrations, services, separately developed technologies, proprietary AI capabilities, or silicon IP may use different licensing or commercial models where appropriate.

The broader Kritva platform may therefore evolve toward a combination of:

- Apache-2.0 community software
- Open developer interfaces
- Research and educational components
- Commercial software
- Enterprise capabilities
- Hardware-specific integrations
- Reference hardware
- Managed services
- Proprietary AI capabilities
- Semiconductor IP
- Licensing
- Royalties

Any such future component will have its applicable licensing terms clearly identified.

> **Public today does not necessarily mean every future Kritva component will remain Apache-2.0.**

---

# Contributing

Kritva welcomes technical feedback, experimentation, ideas, and contributions.

Potential contributors include:

- Robotics engineers
- Embedded engineers
- AI/ML engineers
- Control engineers
- Software engineers
- Simulation engineers
- Hardware engineers
- Semiconductor engineers
- Researchers
- Students
- Robotics enthusiasts

Areas of contribution include:

- Core software
- Robotics middleware
- Sensors
- Motion control
- AI integration
- Simulation
- Hardware abstraction
- FPGA
- Renode
- Verification
- Documentation
- Examples
- Developer tooling

Contribution guidelines will evolve as the project matures.

See [`CONTRIBUTING.md`](CONTRIBUTING.md).

---

# Project Philosophy

The name **KRITVA** is inspired by the idea of creation, making, and bringing capability into existence.

A robotic system represents the convergence of:

```text
Intelligence
     +
Perception
     +
Compute
     +
Body
     +
Control
     +
Action
     +
Learning
```

Kritva aims to connect these elements into an integrated robotic computing platform.

The architectural principle is:

> **From perception to intelligence.**
>
> **From intelligence to action.**
>
> **From action to capability.**
>
> **From capability to scalable robotic systems.**

---

# Long-Term Goal

The long-term goal of Kritva is to establish an open robotic computing architecture that can be reused across different robotic systems and compute implementations.

```text
                 Kritva
                    │
       ┌────────────┴────────────┐
       │                         │
    KritvaOS              Hardware Platform
       │                         │
 ┌─────┼─────┐             ┌─────┴─────┐
 │     │     │             │           │
Sense Mind Motion        Nexus       Edge
 │     │     │             │           │
 └─────┼─────┘             └─────┬─────┘
       │                         │
     Skill                     Subnode
       │                         │
      SDK                    Endpoint
       │                         │
       └────────────┬────────────┘
                    │
                    ▼
             Physical Robot
```

The objective is not to build one robot.

The objective is to create a reusable architecture through which many robotic systems can be built.

---

# Kritva Architectural Principle

> **One Kritva architecture.**
>
> **Multiple robotic systems.**
>
> **Multiple compute implementations.**
>
> **Stable interfaces.**
>
> **Reusable software and hardware IP.**
>
> **Open ecosystem.**
>
> **Validated from simulation to physical robot.**

---

# Project

**KRITVA**

### Open Robotic Computing Platform

**From Physical AI to Real-World Robots.**

GitHub: `https://github.com/KritvaOS/KritvaOS`

Website: `kritvaos.in`

---

## Status

🚧 **Building the foundation for the next generation of robotic computing.**
