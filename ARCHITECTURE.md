# Kritva Architecture

## 1. Purpose

This document defines the system-level architecture of **Kritva**, an open robotic computing platform.

Kritva is designed to connect:

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

The purpose of this architecture is to establish stable system boundaries before implementation details become fixed.

> **Kritva connects intelligence to physical action.**

---

## 2. Vision

Kritva aims to provide a common robotic computing architecture that can be reused across different robotic systems and compute implementations.

The long-term vision is:

> **From Physical AI to Real-World Robots.**

A robot is not treated as only a software application or only a hardware platform.

It is treated as a distributed computing system interacting with the physical world in real time.

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

---

## 3. Architectural Thesis

Modern robots combine multiple technical domains:

- AI
- Perception
- Planning
- Motion
- Embedded systems
- Real-time control
- Networking
- Sensors
- Actuators
- Compute accelerators
- Simulation
- Safety
- Security

These capabilities are frequently developed as separate systems.

Kritva aims to connect these domains through a common architecture.

```text
Application
     ↓
Intelligence
     ↓
Skills
     ↓
Perception / Motion
     ↓
Core
     ↓
Compute
     ↓
Distributed Control
     ↓
Physical Endpoints
```

The architecture is intended to provide continuity from high-level intelligence to low-level physical action.

---

## 4. From Physical AI to Real-World Robots

Physical AI becomes useful only when it can safely interact with the physical world.

Kritva therefore considers the complete path:

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
Actuator
   ↓
Physical Action
```

The reverse path provides sensing and feedback:

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

Together these create a closed robotic loop:

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

## 5. Design Principles

Kritva architecture follows these principles.

### 5.1 Modular

Components should be independently replaceable and extensible.

### 5.2 Hardware Aware

Software should understand the capabilities and constraints of the underlying compute and physical system.

### 5.3 Hardware Agnostic

The architecture should support multiple processor, accelerator, and hardware implementations.

### 5.4 Real-Time Where Required

Time-critical paths should provide deterministic execution where required.

### 5.5 AI-Native

AI is a first-class part of the robotic computing architecture.

### 5.6 Simulation First

Capabilities should be testable in simulation before physical deployment wherever practical.

### 5.7 Safety First

Physical actions must operate within defined software and physical safety boundaries.

### 5.8 Secure by Design

Identity, integrity, communication, access control, and updates are architectural concerns.

### 5.9 Developer Friendly

The platform should make it easy to build, test, debug, simulate, and deploy robotic applications.

### 5.10 Open Architecture

Interfaces should allow hardware, AI models, sensors, applications, and compute platforms to evolve independently.

### 5.11 Reuse Before Reinvention

Mature technologies should be reused where they provide value.

### 5.12 Verify Before Scale

Architectural assumptions should be validated before large-scale implementation.

---

## 6. System Context

Kritva sits between customer applications and physical robotic systems.

```text
Customer Applications
        │
        ▼
    Kritva SDK
        │
        ▼
     KritvaOS
        │
        ▼
Kritva Hardware Platform
        │
        ▼
 Physical Robot
        │
        ▼
 Physical World
```

The platform must support both software-only development and complete hardware-integrated robotic systems.

---

## 7. High-Level System Architecture

```text
                         Customer Applications
                                  │
                                  ▼
 ┌──────────────┐          ┌─────────────┐             ┌──────────────────────┐
 │ Kritva Studio│─────────►│ Kritva SDK  │- - - - - - ►│ Kritva Control Plane │
 │  (planned)   │   uses   │  (planned)  │versioned API│ (planned; separate   │
 └──────────────┘          └──────┬──────┘             │ repository)          │
                                  │                    └──────────────────────┘
                                  ▼
                           ┌─────────────┐
                           │ Kritva Skill│
                           └──────┬──────┘
                                  │
                    ┌─────────────┴─────────────┐
                    ▼                           ▼
             ┌─────────────┐             ┌─────────────┐
             │ Kritva Mind │             │ Kritva Motion│
             │ Intelligence│             │ Control     │
             └──────┬──────┘             └──────┬──────┘
                    │                           │
                    └─────────────┬─────────────┘
                                  ▼
                           ┌─────────────┐
                           │ Kritva Sense│
                           │ Perception  │
                           └──────┬──────┘
                                  │
                                  ▼
                           ┌─────────────┐
                           │ Kritva Core │
                           │ Foundations │
                           └──────┬──────┘
                                  │
                                  ▼
                       Hardware Abstraction
                                  │
                    ┌─────────────┴─────────────┐
                    ▼                           ▼
              Kritva Nexus                 Kritva Edge
             System Compute           Distributed Compute
                    │                           │
                    └─────────────┬─────────────┘
                                  ▼
                               Subnode
                                  │
                                  ▼
                               Endpoint
                                  │
                                  ▼
                         Sensors / Actuators
                                  │
                                  ▼
                           Physical World
```

Kritva Sim and developer tooling operate across these layers.

Planned components are marked "(planned)". Under decisions D-144 and D-149 in kritva-program, Kritva Studio is a planned in-tree component of this repository (`studio/`, not present yet), the Kritva SDK is the separate repository `kritva-sdk` (D-149 supersedes D-144 for the SDK, so no `sdk/` directory is planned here), and the Kritva Control Plane is a planned separate repository (proposed name `kritva-control-plane`, not created). Kritva Sim placement is still open. The Control Plane is drawn beside the stack, with a dashed link to the SDK only; how its API responsibilities and its integration with the capability layers work is left to the architecture task and the Control Plane API contract. The arrows show intended interfaces, not integrations that exist. D-144 settles placement only: it does not move any planned task or authorize creating a repository.

---

## 8. Kritva Computing Stack

The common stack is:

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
Nexus
     ↓
Edge
     ↓
Subnode
     ↓
Endpoint
     ↓
Physical World
```

The architecture deliberately separates:

- Application intent
- Intelligence
- Skills
- Perception
- Motion
- Runtime foundations
- Compute
- Distributed control
- Physical interfaces

---

## 9. KritvaOS

KritvaOS is the open robotic software platform within Kritva.

```text
KritvaOS
│
├── Core
├── Sense
├── Mind
├── Motion
├── Skill
├── Sim
└── SDK
```

KritvaOS is intended to operate across heterogeneous computing environments.

It is not tied to one CPU, GPU, robot, or silicon implementation.

---

## 10. Kritva Core

Kritva Core provides common software foundations shared across the platform.

Core should remain small, stable, and platform-independent.

### 10.1 Core Foundation

The Core Foundation model includes:

- Identity
- Lifecycle
- Status
- Health
- Statistics
- Error and fault handling
- Result
- Events
- Capability
- Configuration
- Version
- Timestamp
- Duration
- Metadata

### 10.2 Lifecycle

A common lifecycle model is:

```text
UNKNOWN
   ↓
INITIALIZING
   ↓
READY
   ↓
RUNNING
   ↓
STOPPING
   ↓
STOPPED
```

Fault handling may transition through:

```text
RUNNING
   ↓
FAULT
   ↓
RECOVERING
   ↓
READY / RUNNING
```

### 10.3 Status and Health

Status and health are intentionally separate.

Example:

```text
Status = RUNNING
Health = DEGRADED
```

A component can remain operational while reporting degraded health.

### 10.4 Statistics

Statistics may include:

Counters:

- sample_count
- error_count
- retry_count
- drop_count

Gauges:

- queue_depth
- temperature
- utilization

### 10.5 Errors

An error should provide structured information such as:

```text
Error
├── code
├── severity
├── source
├── timestamp
└── context
```

### 10.6 Result

Operational failures should be representable through a result model such as:

```text
Result<T>
```

rather than requiring exceptions for normal operational error paths.

### 10.7 Events

A common event envelope may include:

```text
Event
├── event_id
├── source_id
├── event_type
├── timestamp
├── severity
└── correlation_id
```

### 10.8 Capability

Capabilities describe what a component or device supports.

```text
Capability
├── capability_id
├── version
├── name
└── capability_set
```

### 10.9 Configuration

Configuration should be:

- Typed
- Validated
- Versioned
- Hardware-aware
- Environment-aware

Core should not require one specific serialization format.

### 10.10 Time

Core should provide common:

- Timestamp
- Duration
- Monotonic time

Specific synchronization implementations belong at the appropriate platform layer.

---

## 11. Core Architectural Boundary

Core should not become the location for every robotic subsystem.

The intended boundary is:

```text
Kritva Core
│
├── Common Platform Primitives
│
└── Runtime Foundations
        │
        ▼
Hardware Abstraction
        │
        ├── Drivers
        ├── Nexus
        ├── Edge
        └── Endpoint
```

Core should not directly own:

- Robot-specific algorithms
- AI models
- ROS2
- DDS
- EtherCAT implementation
- Hardware-specific drivers
- Cloud services
- Databases
- Customer applications
- Robot-specific motion algorithms

These may integrate with Core through defined interfaces.

---

## 12. Kritva Sense

Kritva Sense provides the perception and sensor framework.

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

Conceptual flow:

```text
Sensors
   ↓
Drivers
   ↓
Sensor Abstraction
   ↓
Kritva Sense
   ↓
Processing
   ↓
Fusion
   ↓
World Representation
```

Sense should provide interfaces that allow perception algorithms and sensor hardware to evolve independently.

---

## 13. Kritva Mind

Kritva Mind provides intelligence and reasoning capabilities.

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

Conceptual flow:

```text
Sense
  ↓
World Understanding
  ↓
Mind
  ↓
Reasoning
  ↓
Planning
  ↓
Skill / Motion
```

Mind should not require a single AI model provider.

---

## 14. Kritva Motion

Kritva Motion provides physical control capabilities.

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

Conceptual flow:

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
Edge
      ↓
Actuator
```

Motion algorithms should remain separate from the low-level hardware abstraction and device drivers.

---

## 15. Kritva Skill

A Skill represents a reusable robot capability.

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

Skills can compose multiple subsystems.

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

The Skill layer provides a bridge between high-level intent and coordinated robot behavior.

---

## 16. Kritva Sim

Kritva Sim provides simulation and validation capabilities.

Potential capabilities include:

- Robot simulation
- Digital twins
- Physics simulation
- Sensor simulation
- Motion validation
- AI testing
- SIL
- HIL
- Regression testing
- Scenario generation
- Failure testing

The intended development principle is:

> **Develop in simulation → Validate → Deploy to the physical robot**

---

## 17. Kritva SDK

The SDK provides developer-facing interfaces.

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
- Examples
- Documentation

Customer APIs should be treated as first-class architecture.

---

## 18. Physical Computing Hierarchy

Kritva defines a physical computing hierarchy:

```text
Robotic System
│
├── Nexus
│   └── System-level compute
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

This hierarchy separates high-level system computation from distributed real-time physical control.

---

## 19. Kritva Nexus

Kritva Nexus is the system-level compute platform.

Typical workloads include:

- AI inference
- Vision
- World models
- Planning
- System coordination
- Global robot state
- Storage
- Security
- Networking
- System management
- Developer applications
- EtherCAT connectivity

A Nexus implementation may contain:

- CPU
- GPU
- NPU
- Vision accelerator
- Other specialized accelerators
- High-speed memory
- Storage
- Networking
- Security hardware

Nexus is a platform family rather than one mandatory physical chip.

---

## 20. Kritva Edge

Kritva Edge is the distributed real-time compute platform.

Potential workloads include:

- Motor control
- Joint control
- Encoder processing
- PWM
- ADC
- GPIO
- SPI
- I2C
- UART
- EtherCAT
- Local diagnostics
- Local safety functions

An Edge implementation may contain:

- Real-time CPU
- MCU-class processing
- Real-time peripherals
- EtherCAT interface
- Local hardware acceleration
- Dedicated control logic

---

## 21. Nexus / Edge Platform Family

Nexus and Edge should be treated as platform families.

The architecture does not assume:

```text
One Nexus chip
+
One Edge chip
=
Every robot
```

Instead:

```text
Common Kritva Architecture
          │
    ┌─────┴─────┐
    ▼           ▼
Nexus Family  Edge Family
    │           │
    ├── Variant A
    ├── Variant B
    └── Variant C
```

Different robots and workloads may require different compute configurations.

The common architecture and interfaces are more important than a single physical implementation.

---

## 22. Subnode

A Subnode is a local compute or aggregation point below Edge.

Possible functions include:

- Local sensor aggregation
- Local actuator aggregation
- Protocol conversion
- Device monitoring
- Local preprocessing
- Endpoint management

A Subnode may be implemented in MCU, FPGA, ASIC, or other suitable hardware.

---

## 23. Endpoint

An Endpoint represents the physical interface to the robot.

Examples include:

- Camera
- IMU
- Encoder
- Motor
- Servo
- Force sensor
- GPIO device
- ADC device
- PWM device
- Other physical sensors and actuators

The Endpoint abstraction separates physical devices from higher-level robotic software.

---

## 24. Distributed Computing

Robotic systems naturally distribute compute.

For example:

```text
                 Nexus
                   │
             Network / EtherCAT
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

The architecture allows compute to be placed according to:

- Latency
- Bandwidth
- Determinism
- Power
- Thermal limits
- Safety
- Cost
- Physical locality

---

## 25. Real-Time Architecture

Not every workload requires hard real-time behavior.

Kritva therefore separates high-level and time-critical workloads.

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

Possible implementation environments include:

- Linux
- PREEMPT_RT
- RTOS
- MCU firmware
- FPGA
- Dedicated hardware

The architecture should not force every workload into one timing model.

---

## 26. Operating System Architecture

KritvaOS may span multiple operating environments.

```text
System-Level Compute
        │
        └── Linux / PREEMPT_RT

Distributed Real-Time Compute
        │
        └── RTOS / Bare Metal / Real-Time Linux

Dedicated Hardware
        │
        └── FPGA / Hardware Logic
```

The software architecture should maintain common interfaces even when implementation environments differ.

---

## 27. Distributed Communication

Kritva requires communication across multiple compute domains.

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

Potential technologies include:

- Ethernet
- EtherCAT
- PCIe
- SPI
- I2C
- UART
- GPIO
- Other platform-specific transports

No single transport is assumed to be appropriate for every layer.

---

## 28. Nexus / Edge Communication

A typical architecture may use:

```text
Nexus
  │
  │ Ethernet / EtherCAT
  ▼
Edge
  │
  │ Local bus
  ▼
Subnode
  │
  ▼
Endpoint
```

The communication architecture should distinguish between:

- High-bandwidth communication
- Low-latency communication
- Deterministic control
- Configuration
- Diagnostics
- Telemetry
- Safety signals

---

## 29. Hardware Abstraction

Kritva separates software interfaces from hardware implementations.

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

Hardware-specific implementation details should remain below stable interfaces wherever practical.

---

## 30. Hardware / Software Co-Design

Robotic computing increasingly requires hardware and software to be designed together.

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

Potential optimization targets include:

- AI inference
- Vision
- Sensor processing
- Real-time control
- Motion computation
- Networking
- Security
- Power efficiency

---

## 31. FPGA and Silicon Strategy

Future Kritva hardware implementations may progress through:

```text
Architecture
    ↓
RTL
    ↓
Simulation
    ↓
Renode / Virtual Platform
    ↓
FPGA
    ↓
HIL
    ↓
Silicon
```

The software architecture should be designed so that hardware implementations can evolve without forcing unnecessary changes into upper software layers.

---

## 32. Simulation and Validation Architecture

The validation path is:

```text
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
```

Validation should happen as early as possible.

Simulation should not be treated as a separate development activity disconnected from physical deployment.

---

## 33. Renode

Renode may provide a virtual hardware environment for:

- Firmware development
- Peripheral validation
- Software integration
- Boot testing
- Driver testing
- System regression
- Hardware/software co-development

The same APIs should be exercised across virtual and physical environments wherever practical.

---

## 34. ROS2 Integration

Kritva is not intended to unnecessarily replace established robotics ecosystems.

ROS2 can provide:

- Robotics middleware
- Communication
- Tooling
- Visualization
- Ecosystem integration
- Research infrastructure
- Application development

Kritva can integrate with ROS2 while providing a broader robotic computing architecture.

```text
Kritva
│
├── KritvaOS
├── Kritva Hardware
└── ROS2 Integration
```

Potential integration technologies include:

- ROS2
- `ros2_control`
- DDS
- micro-ROS

The goal is interoperability rather than unnecessary reinvention.

---

## 35. AI Architecture

Kritva AI architecture separates models from the underlying robotic system.

```text
AI Models
   ↓
Inference Runtime
   ↓
Kritva Mind
   ↓
World Model
   ↓
Reasoning / Planning
   ↓
Skill
   ↓
Motion
```

AI models may run on:

- CPU
- GPU
- NPU
- Other accelerators

The architecture should avoid making one AI model or vendor a mandatory platform dependency.

---

## 36. Hardware-Aware AI

Physical AI must account for the capabilities and constraints of the physical platform.

Relevant constraints include:

- Compute capacity
- Memory
- Latency
- Power
- Thermal budget
- Network bandwidth
- Sensor rate
- Actuator rate
- Safety constraints

The architecture therefore connects:

```text
AI Workload
     ↕
Compute Capability
     ↕
Robot Capability
```

---

## 37. Safety Architecture

Safety should be layered.

```text
Application Safety
       ↓
Skill Safety
       ↓
Motion Safety
       ↓
Real-Time Control Safety
       ↓
Hardware Safety
       ↓
Physical Limits
```

Potential mechanisms include:

- Operational limits
- Motion limits
- Actuator limits
- Fault detection
- Fault reporting
- Watchdogs
- Emergency behavior
- Communication failure handling
- Sensor failure handling
- Compute failure handling
- Recovery mechanisms

Safety-critical paths should remain isolated from non-critical AI and application workloads.

---

## 38. Security Architecture

Security should cover the full system.

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

Security should be considered from architecture through deployment.

---

## 39. Identity and Lifecycle

Every major component should have a consistent identity and lifecycle model where appropriate.

```text
Identity
   ↓
Initialization
   ↓
Capability Discovery
   ↓
Ready
   ↓
Running
   ↓
Health Monitoring
   ↓
Fault / Recovery
   ↓
Shutdown
```

This model should apply consistently across software components and physical compute nodes where practical.

---

## 40. Status, Health, and Diagnostics

The architecture separates:

```text
Status
Health
Statistics
Errors
Events
Diagnostics
```

Example:

```text
Status      = RUNNING
Health      = DEGRADED
Temperature = HIGH
Errors      = 2
Drops       = 5
```

This structured information should be available for local and remote diagnostics.

---

## 41. Observability

Robotic failures can be difficult to reproduce.

Kritva should therefore provide structured observability.

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

Observability should support:

- Development
- Debugging
- Simulation
- Validation
- Production diagnostics
- Fleet monitoring
- Failure analysis

---

## 42. Configuration Architecture

Configuration should be:

- Typed
- Versioned
- Validated
- Discoverable
- Hardware-aware
- Environment-aware

Kritva Core should not require a specific serialization format.

Higher layers may use:

- YAML
- JSON
- TOML
- Databases
- Other formats

These formats should remain implementation or tooling choices rather than Core API requirements.

---

## 43. Time Architecture

Robotic systems depend heavily on time.

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

Core should provide common timestamp and duration abstractions.

Specific synchronization technologies may include:

- Monotonic clocks
- Hardware timestamps
- PTP
- EtherCAT distributed clocks
- Other platform-specific synchronization

The implementation belongs at the appropriate system layer.

---

## 44. Storage Architecture

Kritva does not require a mandatory centralized database or cloud architecture.

Storage may be provided for:

- Configuration
- Logs
- Diagnostics
- AI models
- Calibration
- Maps
- Mission data
- Telemetry
- Historical information

Storage implementations remain deployment-specific.

---

## 45. Robot-Type Architecture

The common Kritva architecture is robot-type independent.

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

Robot-specific specialization occurs in:

- Sense
- Motion
- Skill
- Edge configuration
- Hardware
- Endpoint topology

---

## 46. Humanoid Architecture

Humanoids may use multiple distributed Edge platforms.

```text
                 Nexus
                   │
       ┌───────────┼───────────┐
       ▼           ▼           ▼
      Arm         Leg         Head
       │           │           │
     Edge        Edge        Edge
       │           │           │
   Subnodes    Subnodes    Subnodes
       │           │           │
   Endpoints   Endpoints   Endpoints
```

Potential requirements include:

- High sensor density
- Whole-body control
- Distributed motion control
- Balance
- Manipulation
- AI inference
- High-level reasoning

---

## 47. Manipulator / Cobot Architecture

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

---

## 48. Mobile Robot Architecture

```text
Nexus
  │
  ├── Edge → Drive
  ├── Edge → Sensors
  └── Edge → Actuators
```

Focus areas include:

- Navigation
- Localization
- Perception
- Drive control

---

## 49. Quadruped Architecture

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

---

## 50. Aerial Architecture

Aerial robots may use specialized compute optimized for:

- Weight
- Power
- Flight control
- Sensor fusion
- Navigation
- AI inference

The physical implementation may differ while maintaining the common Kritva architectural model.

---

## 51. One Architecture, Multiple Robots

Kritva should maintain common interfaces while allowing robot-specific implementations.

```text
Common Kritva Architecture
          │
    ┌─────┼─────┬─────┬─────┐
    ▼     ▼     ▼     ▼     ▼
Humanoid  Cobot  Mobile  Quad  Aerial
```

The objective is reuse rather than forcing every robot to have identical hardware.

---

## 52. Extensibility

The architecture should support future:

- Sensors
- Actuators
- Compute platforms
- AI models
- Robotics algorithms
- Communication technologies
- Hardware implementations
- Simulation environments

New capabilities should integrate through stable interfaces.

---

## 53. API Architecture

APIs should be organized by responsibility.

```text
Core APIs
   ↓
Hardware APIs
   ↓
Sense / Motion APIs
   ↓
Mind APIs
   ↓
Skill APIs
   ↓
SDK APIs
```

API stability should increase as the architecture matures.

Early APIs may change significantly.

Stable customer-facing interfaces should be separated from internal implementation details.

---

## 54. Architectural Boundaries

Kritva should maintain clear boundaries between:

```text
Application
     │
     ▼
SDK
     │
     ▼
Skills
     │
     ├──────────────┐
     ▼              ▼
Mind             Motion
     │              │
     └──────┬───────┘
            ▼
          Sense
            │
            ▼
           Core
            │
            ▼
 Hardware Abstraction
            │
       ┌────┴────┐
       ▼         ▼
     Nexus      Edge
                  │
                Subnode
                  │
               Endpoint
```

Each layer should have a clear responsibility and dependency direction.

---

## 55. External Ecosystem

Kritva should integrate with existing ecosystems where practical.

Potential integrations include:

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
```

Kritva does not need to replace every component of the robotics stack.

---

## 56. No Unnecessary Reinvention

The architecture follows:

> **Reuse mature technologies where they provide value.**

Kritva should not unnecessarily recreate:

- Operating systems
- Robotics middleware
- AI frameworks
- Network protocols
- Hardware standards
- Simulation engines

Instead, Kritva should provide the architecture and interfaces that connect these technologies.

---

## 57. IP Architecture

Kritva may eventually span multiple IP layers.

```text
Kritva Architecture
        │
        ├── Software IP
        ├── Runtime IP
        ├── Robotics IP
        ├── Hardware IP
        ├── FPGA IP
        ├── Accelerator IP
        └── Silicon IP
```

The public software platform and future commercial IP may have different licensing models.

Any future commercial component should have clearly defined licensing terms.

---

## 58. Customer-Specific Architecture

Kritva should allow customer-specific implementations without fragmenting the common architecture.

```text
Common Kritva Architecture
          │
    ┌─────┴─────┐
    ▼           ▼
Customer A   Customer B
    │           │
Customized   Customized
Hardware     Hardware
    │           │
    └─────┬─────┘
          ▼
   Common Interfaces
```

Customer-specific engineering should ideally contribute reusable architecture and IP back into the platform where commercially and technically appropriate.

---

## 59. Architecture Reuse

The architecture should maximize reuse across:

- Robot types
- Compute platforms
- Hardware implementations
- Simulation
- FPGA
- Silicon
- Customer programs

The key reusable assets are:

```text
Architecture
Interfaces
APIs
IP
Verification
Tools
```

---

## 60. Development Methodology

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

AI-assisted engineering may support:

- Code generation
- Code review
- Test generation
- Documentation
- Static analysis
- Architecture review
- Verification assistance

Human engineering review remains responsible for architectural and safety decisions.

---

## 61. Verification Architecture

Verification occurs at multiple levels:

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

Each stage should provide progressively stronger evidence.

---

## 62. Repository Architecture Relationship

The system architecture is defined first.

```text
ARCHITECTURE.md
      ↓
Component Boundaries
      ↓
Repository Architecture
      ↓
Implementation
      ↓
Verification
```

The repository architecture should explain:

- Where components live
- Ownership
- Dependencies
- Git submodule boundaries
- Release integration
- Repository split criteria

Detailed repository organization belongs in:

`docs/architecture/REPOSITORY_ARCHITECTURE.md`

---

## 63. Repository Architecture

The initial repository may use a modular monorepo:

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

Major components may become independent repositories or Git submodules when ownership, reuse, interface stability, or release lifecycle justify the split.

The project should avoid premature fragmentation.

---

## 64. Deployment Architecture

A reference deployment may look like:

```text
                 Applications
                      │
                    SDK
                      │
                Skill / Mind
                      │
             ┌────────┴────────┐
             │                 │
           Sense             Motion
             │                 │
             └────────┬────────┘
                      │
                    Core
                      │
             Hardware Abstraction
                      │
                    Nexus
                      │
              Ethernet / EtherCAT
                      │
          ┌───────────┼───────────┐
          ▼           ▼           ▼
        Edge        Edge        Edge
          │           │           │
       Subnodes    Subnodes    Subnodes
          │           │           │
       Endpoints   Endpoints   Endpoints
```

The actual topology depends on the robot.

---

## 65. First Vertical Slice

The highest-priority early objective is to prove one complete path from software to physical action.

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

This is more important than attempting to implement the complete robotic ecosystem immediately.

---

## 66. v0.1 Scope

The first meaningful architecture milestone should focus on:

### Software

- Kritva Core Foundation
- Basic runtime abstractions
- Lifecycle
- Status
- Health
- Statistics
- Errors
- Events
- Capability
- Configuration
- Time
- Initial hardware abstraction

### Hardware

- Nexus interface
- Edge interface
- One distributed communication path
- One endpoint path

### Validation

- Unit tests
- Contract tests
- Integration tests
- Simulation
- Renode
- Initial FPGA/HIL path where practical

### Documentation

- System architecture
- Repository architecture
- Requirements
- API definitions
- Verification plan
- ADR framework

---

## 67. v0.1 Non-Goals

The first milestone should not attempt to complete:

- Full humanoid autonomy
- Full general-purpose AI
- Complete perception stack
- Complete locomotion stack
- Complete simulation ecosystem
- Production silicon
- Full fleet management
- Complete cloud platform
- Every robot type
- Every sensor and actuator
- Every communication protocol

The purpose of v0.1 is to prove the architecture.

---

## 68. Success Criteria

The architecture should be considered successfully demonstrated when:

1. A software application can invoke a Kritva capability.
2. The capability can cross the defined software layers.
3. The request reaches a distributed compute platform.
4. A real-time Edge path can execute the physical control.
5. Sensor feedback can return through the architecture.
6. The same interfaces can be exercised in simulation.
7. The implementation can be tested in Renode or equivalent virtual hardware where applicable.
8. The architecture remains independent of one specific robot implementation.

---

## 69. Engineering Traceability

Requirements should be traceable through implementation and verification.

```text
Requirement
    ↓
Architecture
    ↓
API
    ↓
Source File
    ↓
Test
    ↓
Verification Result
```

This traceability becomes increasingly important for:

- Safety
- Security
- Real-time behavior
- Hardware interfaces
- Customer requirements
- Production deployment

---

## 70. Architecture Evolution

Kritva is in an early architecture phase.

Therefore:

- Interfaces may change.
- Module boundaries may change.
- Hardware architecture may change.
- APIs may change.
- Repository boundaries may change.
- Implementation technologies may change.

However, changes should be documented and intentional.

Architecture evolution should preserve stable principles even when implementations change.

---

## 71. Architecture Decision Records

Important architectural decisions should be captured as ADRs.

Examples:

```text
docs/adr/
├── ADR-0001-architecture-principles.md
├── ADR-0002-core-boundary.md
├── ADR-0003-nexus-edge-model.md
├── ADR-0004-repository-model.md
├── ADR-0005-ros2-integration.md
└── ADR-0006-real-time-model.md
```

An ADR should explain:

- Context
- Problem
- Decision
- Alternatives
- Consequences
- Status

---

## 72. Architecture Governance

Architecture changes should consider:

- Technical correctness
- Interface stability
- Verification impact
- Safety impact
- Security impact
- Hardware impact
- Customer impact
- Long-term reuse

Major architectural changes should be documented before implementation where practical.

---

## 73. Development and Team Structure

Development should be organized around bounded technical work packages.

Potential workstreams include:

```text
Core
Hardware Abstraction
Sense
Motion
Mind
Skill
Simulation
Nexus
Edge
FPGA
Renode
Verification
SDK
Documentation
```

Each workstream should have:

- Requirements
- Defined interfaces
- Implementation scope
- Tests
- Review criteria
- Ownership

This allows multiple contributors to work without weakening architectural boundaries.

---

## 74. Customer and Platform Evolution

Kritva may initially be developed through engineering projects and customer-specific programs.

The architecture should ensure that customer engineering produces reusable platform assets where appropriate.

```text
Customer Requirement
       ↓
Engineering
       ↓
Platform Interface
       ↓
Reusable IP
       ↓
Reference Implementation
       ↓
Future Customer
```

The architecture should avoid becoming permanently dependent on one customer implementation.

---

## 75. Long-Term Platform Evolution

Kritva can evolve through stages:

```text
Engineering Foundation
        ↓
Reference Platform
        ↓
Customer Programs
        ↓
Reusable IP
        ↓
Robotic Platform
        ↓
Robotic Computing Architecture
```

The architecture should support this evolution without requiring a complete redesign at each stage.

---

## 76. Long-Term Goal

The long-term goal is to establish an open robotic computing architecture that can be reused across different robotic systems and compute implementations.

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

## 77. Final Architectural Principle

Kritva is built around one fundamental principle:

> **Connect intelligence to physical action through an open, hardware-aware robotic computing architecture.**

The resulting architecture is:

```text
Physical AI
     ↓
KritvaOS
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
```

And the platform principle is:

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

## Related Documents

- [`README.md`](README.md)
- [`docs/architecture/REPOSITORY_ARCHITECTURE.md`](docs/architecture/REPOSITORY_ARCHITECTURE.md)
- [`docs/architecture/SOC_ARCHITECTURE.md`](docs/architecture/SOC_ARCHITECTURE.md)
- [`docs/architecture/FPGA_ARCHITECTURE.md`](docs/architecture/FPGA_ARCHITECTURE.md)
- [`docs/architecture/RENODE_ARCHITECTURE.md`](docs/architecture/RENODE_ARCHITECTURE.md)
- [`docs/architecture/DISTRIBUTED_COMPUTING.md`](docs/architecture/DISTRIBUTED_COMPUTING.md)
- [`docs/architecture/REALTIME_ARCHITECTURE.md`](docs/architecture/REALTIME_ARCHITECTURE.md)
- [`docs/architecture/SAFETY_ARCHITECTURE.md`](docs/architecture/SAFETY_ARCHITECTURE.md)
- [`docs/architecture/SECURITY_ARCHITECTURE.md`](docs/architecture/SECURITY_ARCHITECTURE.md)
- [`docs/requirements/`](docs/requirements/)
- [`docs/api/`](docs/api/)
- [`docs/verification/`](docs/verification/)
- [`docs/adr/`](docs/adr/)

---

## Status

**Architecture Status:** Early Architecture / Pre-Alpha

This document defines the current architectural direction. It is expected to evolve as Kritva moves from architecture validation to implementation, hardware validation, customer programs, and production systems.
