# KOS-I3 — Device & Endpoint Integration

**Status:** PLANNED  
**Repository:** `kritvaos-community`  
**Dependency:** `kritva-core` R1.0  
**Predecessor:** KOS-I2 — Runtime / Application Foundation

## 1. Purpose

Establish the first stable hardware-facing abstraction above the KOS-I2 runtime: **Device** and **Endpoint**.

I3 shall allow a Kritva application to discover, configure, operate, observe, and fault-inject simulated physical devices without introducing a dependency on a particular bus, driver, MCU, SoC, or transport.

## 2. Objective

Demonstrate the following vertical slice using mock hardware only:

```text
Runtime
  ↓
Device Registry
  ↓
Device
  ↓
Endpoint
  ↓
Typed Sensor / Actuator Operation
  ↓
Mock Hardware
```

The reference flow is:

```text
Create Runtime
  ↓
Register Device
  ↓
Discover Endpoint
  ↓
Configure Endpoint
  ↓
Initialize
  ↓
Start
  ↓
Read Sensor Data / Write Actuator Command
  ↓
Observe Status / Health / Statistics
  ↓
Inject Endpoint Fault
  ↓
Observe Failure
  ↓
Controlled Shutdown
```

## 3. Scope

### In scope

- Device identity and lifecycle contract.
- Endpoint identity and lifecycle contract.
- Device/Endpoint capability metadata.
- Device registry and deterministic discovery.
- Runtime integration of devices.
- Typed sensor read and actuator write contracts.
- Mock Device/Endpoint implementations.
- Endpoint statistics and diagnostics.
- Deterministic fault injection.
- Unit, integration, sanity, and regression verification.
- Reference Device/Endpoint demo.

### Explicitly out of scope

- EtherCAT.
- CAN/CAN-FD.
- SPI/I2C/UART/GPIO/PWM/ADC hardware drivers.
- STM32 or other MCU firmware.
- Nexus/Edge transport.
- Subnode firmware.
- PREEMPT_RT.
- ROS2/DDS/ros2_control.
- Vendor SDKs.
- Physical robot hardware.
- Real-time motor control algorithms.
- Sensor fusion, perception, AI, Mind, Motion, Skill, or SDK product features.

## 4. Architectural Principle

> **I3 defines what a physical Device and Endpoint are. I4 defines how an Endpoint can be reached remotely.**

I3 must therefore be transport-neutral.

```text
I3

Application
    ↓
KOS-I2 Runtime
    ↓
Device Registry
    ↓
Device
    ↓
Endpoint
    ↓
Mock Implementation
```

I4 may extend the same contract:

```text
Application
    ↓
Runtime
    ↓
Device
    ↓
Endpoint
    ↓
Nexus ↔ Edge Transport
    ↓
Remote Endpoint
```

## 5. Task Summary

| Task | Description | Dependency |
|---|---|---|
| I3-001 | Device / Endpoint architecture and contracts | KOS-I2 |
| I3-002 | Device and Endpoint registry | I3-001 |
| I3-003 | Endpoint lifecycle and Runtime integration | I3-001, I3-002 |
| I3-004 | Typed sensor / actuator data interfaces | I3-001 |
| I3-005 | Mock Device / Endpoint implementation | I3-002, I3-004 |
| I3-006 | Observation, diagnostics and fault handling | I3-003, I3-005 |
| I3-007 | Reference Device/Endpoint demo | I3-001..006 |

## 6. Completion Criteria

KOS-I3 is complete when:

1. all seven tasks are implemented and accepted;
2. Device and Endpoint contracts are documented and tested;
3. duplicate and missing discovery operations fail deterministically;
4. Device/Endpoint lifecycle is coordinated without duplicating Core lifecycle machinery;
5. typed sensor reads and actuator writes work with mock endpoints;
6. endpoint status, health, statistics, and failures are observable;
7. fault injection produces deterministic failure evidence;
8. the reference demo completes the complete I3 flow;
9. all I2 regression tests remain passing;
10. clean build, sanity, regression, review, and documentation gates pass;
11. each task has an atomic commit and recorded evidence;
12. no Core R1.0 source/API modification is required.
