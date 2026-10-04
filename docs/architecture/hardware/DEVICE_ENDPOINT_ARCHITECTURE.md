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

## 10. Contracts as built (I3-001)

Library `kritva_hardware` (`hardware/abstraction/`, namespace `kritva::hardware`); it depends only on `kritva-core`.

| Type | Role |
|---|---|
| `DeviceId`, `EndpointId` | Distinct strong ids over Core `Id` semantics; valid iff non-zero. |
| `DeviceInfo`, `EndpointInfo` | Immutable identity: valid id and a unique name of 1..64 characters of `[a-z0-9_]` (the dot is reserved for `<device>.<endpoint>.<setting>` configuration keys). `EndpointInfo` also records the direction (sensor or actuator). |
| `Endpoint` | One capability of a Device. Implements the Core Component lifecycle table once, on a released Core `Lifecycle` (no new state machine); implementations supply hooks. Exposes status, health, capabilities, statistics (successful and failed data operations) and `last_error()`. |
| `SensorEndpoint<Sample>` | Adds `read(Sample&)`. |
| `ActuatorEndpoint<Command>` | Adds `write(const Command&)`. |
| `Device` | Identity plus the Endpoints it owns (insertion order = enumeration order). No lifecycle of its own; status, health and capabilities are aggregated from its endpoints. |

Rules:

- **Lifecycle.** `configure` and `initialize` from UNKNOWN or STOPPED; `start` from READY; `stop` from READY or RUNNING; `shutdown` from UNKNOWN or STOPPED (releases a live period once) and from FAULT (to STOPPED). Anything else fails with `INVALID_STATE` and changes nothing. A failing `initialize`, `start` or `stop` hook leaves the endpoint in FAULT and its error is returned unchanged. FAULT is left only by `shutdown()`; nothing recovers an endpoint by itself.
- **Errors** use released Core codes: data operation while not RUNNING `NOT_READY`; on a FAULT endpoint `RESOURCE_UNAVAILABLE`; injected hardware failure `INTERNAL_ERROR`; invalid argument or out-of-range command `INVALID_ARGUMENT`; invalid lifecycle call `INVALID_STATE`. Endpoints leave `Error::source` unset; the owning DeviceManager sets its component id.
- **Data operations** are refused without calling the implementation unless RUNNING. A failing `do_read`/`do_write` is counted and recorded but does not by itself fault the endpoint; an implementation calls `enter_fault()` for that.
- **Data types** must be trivially copyable value types; there is no universal sample type.
- **Ownership.** The integrator owns the Device; the Device owns its Endpoints; the endpoint set is fixed once any endpoint has left UNKNOWN. A Device is not a Core Component.
- **Timing.** Control plane only: operations may allocate and block, and no real-time guarantee is made.

Verification support: `tests/hardware/conformance/endpoint_conformance.hpp` is a reusable suite (exhaustive lifecycle table, observation, no recovery, data-operation errors) that every Endpoint implementation, including the I3-005 mocks, must pass unchanged.

### Device registry (I3-002)

`DeviceRegistry` discovers Devices and, through them, Endpoints. It does not own Devices (the integrator does), enumerates in registration order, rejects a duplicate device id or name, and finds endpoints by Device and Endpoint id or by names (endpoint ids are unique only within a Device). A missing device or endpoint fails with `INVALID_ARGUMENT` and a message naming it. Registration is closed with `close()` (the DeviceManager does this at the first initialize); there is no unregistration. It is separate from the Core component registry.
