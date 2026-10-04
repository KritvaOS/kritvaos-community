# KOS-I3 Implementation Plan

## 1. Implementation Strategy

I3 extends the KOS-I2 runtime downward toward hardware without changing Core R1.0.

The implementation shall introduce a small hardware abstraction under:

```text
hardware/
└── abstraction/
    ├── include/kritva/hardware/
    ├── src/
    └── CMakeLists.txt
```

Mock implementations belong under test support:

```text
tests/hardware/mock/
```

The reference executable belongs under:

```text
examples/device_demo/
```

## 2. Contract Boundary

```text
KOS-I2 Runtime
      │
      ▼
Device Registry / Device Manager
      │
      ▼
Device
      │
      ▼
Endpoint
      │
      ├── Sensor endpoint
      │      └── typed read
      │
      └── Actuator endpoint
             └── typed write
```

Device/Endpoint code may use released Core types such as:

- `StatusCode`
- `HealthState`
- `Result`
- `Error`
- statistics/event contracts where appropriate

Do not re-create these types.

## 3. I3-001 — Device / Endpoint Architecture and Contracts

Define:

- Device identity.
- Endpoint identity.
- endpoint kind/capability.
- ownership relationship: Device owns/contains Endpoint references.
- configuration contract.
- lifecycle operations.
- status and health access.
- error semantics.
- deterministic behavior requirements.

Recommended public interfaces:

```text
Device
Endpoint
SensorEndpoint
ActuatorEndpoint
```

The base `Endpoint` shall not contain sensor/actuator-specific data methods.

Specialized endpoint interfaces shall expose typed operations.

Do not create a universal `SensorData`/`ActuatorData` variant containing every current or future robot data type.

## 4. I3-002 — Device and Endpoint Registry

Implement a deterministic registry with:

- register device;
- unregister device where required by ownership model;
- find device by stable ID;
- find endpoint by Device ID + Endpoint ID;
- duplicate Device ID rejection;
- duplicate Endpoint ID rejection within a Device;
- missing lookup errors;
- deterministic enumeration.

The registry is distinct from the Core runtime component registry.

## 5. I3-003 — Endpoint Lifecycle and Runtime Integration

Integrate Device/Endpoint operation with `RuntimeHost`.

Rules:

- Device/Endpoint shall reuse Core status/health/result contracts.
- Do not create a second global lifecycle state machine.
- Runtime startup shall initialize registered devices/endpoints in deterministic order.
- Runtime start shall start them in deterministic order.
- Runtime stop shall stop them in reverse dependency/registration order as defined by the implementation.
- Endpoint failure shall become observable without requiring Core R1.0 changes.
- Invalid lifecycle operations shall fail deterministically.

The exact ownership API shall be selected during implementation review, but must preserve the boundary above.

## 6. I3-004 — Typed Sensor / Actuator Data Interfaces

Provide minimal typed contracts sufficient for the reference demo.

Reference data types:

```text
ImuSample
EncoderSample
MotorCommand
```

Reference operations:

```text
ImuEndpoint::read(ImuSample&)
EncoderEndpoint::read(EncoderSample&)
MotorCommandEndpoint::write(const MotorCommand&)
```

Data contracts shall define:

- units;
- validity;
- initialization/default state;
- timestamp policy;
- deterministic error behavior.

Do not introduce real-time guarantees in I3.

## 7. I3-005 — Mock Device / Endpoint Implementation

Implement:

```text
MockDevice
 ├── MockImuEndpoint
 ├── MockEncoderEndpoint
 └── MockMotorEndpoint
```

Mocks shall support:

- deterministic nominal data;
- configurable data sequence;
- successful actuator write;
- read failure injection;
- write failure injection;
- health degradation/fault;
- operation counters/statistics.

Mocks shall not depend on physical drivers or transport libraries.

## 8. I3-006 — Observation, Diagnostics and Fault Handling

Expose, at minimum:

- Device identity;
- Endpoint identity;
- status;
- health;
- capability;
- operation counts;
- last operation result/error;
- fault state;
- deterministic failure reason.

Fault handling shall use existing Core/runtime failure reporting mechanisms where applicable.

A failed endpoint shall not silently recover.

A faulted endpoint shall remain faulted until an explicitly supported reset/reinitialization path exists. I3 does not require automatic recovery.

## 9. I3-007 — Reference Demo

Create:

```text
examples/device_demo/
```

Reference topology:

```text
Device: left_arm_imu
  ├── acceleration
  └── angular_velocity

Device: shoulder_motor
  ├── command
  └── position
```

Demo sequence:

```text
configure
  ↓
register devices
  ↓
discover endpoints
  ↓
initialize
  ↓
start
  ↓
read IMU
  ↓
write motor command
  ↓
observe status/health/statistics
  ↓
inject endpoint fault
  ↓
observe failure
  ↓
controlled shutdown
```

The demo must run deterministically on the standard Linux development host.

## 10. Required Verification

Every task:

```text
Build
  ↓
Unit
  ↓
Integration
  ↓
Sanity
  ↓
Regression
  ↓
Review
  ↓
Atomic Commit
```

Regression must include the complete KOS-I2 suite plus applicable I3 tests.

## 11. Implementation Restrictions

Do not:

- modify `core/`;
- add EtherCAT/CAN/ROS2/DDS;
- add hardware-specific drivers;
- add MCU firmware;
- add PREEMPT_RT dependencies;
- introduce transport abstractions needed only by I4;
- add speculative generic data registries;
- duplicate Core status/health/result/error types;
- move KOS-I2 code without a documented architectural reason.
