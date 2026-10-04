# KritvaOS Device and Endpoint Architecture

**Introduced by:** KOS-I3  
**Dependency:** `kritva-core` R1.0

## 1. Purpose

Define the hardware-facing abstraction boundary between the KOS-I2 runtime and future physical drivers/transports.

## 2. Architectural Position

```text
Application
    ↓
SDK / Skill / higher layers
    ↓
KOS Runtime
    ↓
Device Registry / Device Manager
    ↓
Device
    ↓
Endpoint
    ↓
Hardware Abstraction
    ↓
Driver / Platform
    ↓
Hardware
```

I3 implements only through Mock Hardware:

```text
Runtime
  ↓
Device Registry
  ↓
Device
  ↓
Endpoint
  ↓
Mock
```

## 3. Device

A Device represents a physical or simulated physical unit.

Examples:

- IMU device
- motor device
- encoder device
- camera device

A Device owns or exposes a collection of Endpoints.

## 4. Endpoint

An Endpoint represents one physical capability/interface of a Device.

Examples:

- acceleration
- angular velocity
- position
- velocity
- motor command
- GPIO
- ADC

The Endpoint is the architectural boundary that later permits a physical or remote implementation without changing higher-level application semantics.

## 5. Device vs Core Component

Device and Endpoint are **not** Core runtime Components.

```text
Core Component Registry
    → application/runtime components

Device Registry
    → physical/simulated devices
```

The two registries serve different purposes and must not be merged merely for convenience.

## 6. Lifecycle

Device/Endpoint lifecycle is coordinated by the runtime and uses released Core status/health/result contracts.

I3 does not introduce a second global lifecycle engine.

## 7. Typed Data

Sensor and actuator interfaces are specialized:

```text
SensorEndpoint
    └── typed sample

ActuatorEndpoint
    └── typed command
```

I3 intentionally avoids a universal data variant containing every current and future robotics data type.

## 8. I3/I4 Boundary

I3:

```text
Endpoint → Mock/Local implementation
```

I4:

```text
Endpoint → Nexus/Edge transport → Remote implementation
```

Transport identity, serialization, discovery across nodes, EtherCAT mapping, and network failure semantics belong to I4 or later work.

## 9. Recommended Repository Placement

```text
hardware/
└── abstraction/
    ├── include/kritva/hardware/
    ├── src/
    └── CMakeLists.txt

tests/
└── hardware/
    └── mock/

examples/
└── device_demo/
```

Physical drivers remain under `drivers/` and platform-specific implementation remains under `soc/edge/` or the appropriate hardware area.
