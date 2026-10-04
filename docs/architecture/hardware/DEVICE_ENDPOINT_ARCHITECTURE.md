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

hardware/
└── mock/               (library kritva_hardware_mock; shared by tests and examples)

tests/
└── hardware/           (unit, integration, sanity, conformance, support)

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

- **Lifecycle.** `configure` and `initialize` from UNKNOWN or STOPPED; `start` from READY; `stop` from READY or RUNNING; `shutdown` from UNKNOWN or STOPPED (releases a live period once) and from FAULT (to STOPPED). Anything else fails with `INVALID_STATE` and changes nothing. A failing `initialize`, `start` or `stop` hook leaves the endpoint in FAULT and its error is returned unchanged. A hook that faults the endpoint itself (`enter_fault()`) also fails the operation, with no second transition or notification. `enter_fault()` accepts only a READY or RUNNING endpoint (`INVALID_STATE` otherwise). On every transition into FAULT the endpoint calls the protected `on_fault()` hook (an actuator makes its output safe there) and then its fault listeners, once per fault. Listeners are owner-scoped: each registrant sets and clears only its own. FAULT is left only by `shutdown()`; nothing recovers an endpoint by itself.
- **Errors** use released Core codes: data operation while not RUNNING `NOT_READY`; on a FAULT endpoint `RESOURCE_UNAVAILABLE`; injected hardware failure `INTERNAL_ERROR`; invalid argument or out-of-range command `INVALID_ARGUMENT`; invalid lifecycle call `INVALID_STATE`. Endpoints leave `Error::source` unset; the owning DeviceManager sets its component id.
- **Data operations** are refused without calling the implementation unless RUNNING. A failing `do_read`/`do_write` is counted and recorded but does not by itself fault the endpoint; an implementation calls `enter_fault()` for that. `SensorEndpoint::read()` fills a temporary and enforces the sample's `is_valid()`: a sample that is not valid is an `INTERNAL_ERROR`, and the caller's sample object is written only when the read succeeds.
- **Data types** must be trivially copyable value types (a sensor sample also provides `is_valid()`); there is no universal sample type.
- **Ownership.** The integrator owns the Device; the Device owns its Endpoints; the endpoint set is fixed once any endpoint has left UNKNOWN or the Device has been sealed (the DeviceManager seals every registered Device when it closes the registry). A Device is not a Core Component, and compile-time assertions in the tests keep it and `Endpoint` from deriving from one.
- **Timing.** Control plane only: operations may allocate and block, and no real-time guarantee is made.

Verification support: `tests/hardware/conformance/endpoint_conformance.hpp` is a reusable suite (exhaustive lifecycle table, observation, no recovery, data-operation errors) that every Endpoint implementation, including the I3-005 mocks, must pass unchanged.

### Device registry (I3-002)

`DeviceRegistry` discovers Devices and, through them, Endpoints. It does not own Devices (the integrator does), enumerates in registration order, rejects a duplicate device id or name, and finds endpoints by Device and Endpoint id or by names (endpoint ids are unique only within a Device). A missing device or endpoint fails with `INVALID_ARGUMENT` and a message naming it. Registration is closed with `close()` (the DeviceManager does this at the first initialize); there is no unregistration. It is separate from the Core component registry.

### DeviceManager (I3-003)

`DeviceManager` is the single Core `Component` through which the runtime reaches Devices (`RuntimeHost::add_component`; RuntimeHost, RuntimeManager and Core are unchanged; there is no `add_device`). It runs the Core Component lifecycle table on a Core `Lifecycle` and fans each operation out to the endpoints of the enabled devices: configure, initialize and start in registration order, stop and shutdown in the exact reverse.

- **Configuration.** Keys are `<device>.enabled` (bool, default true) and `<device>.<endpoint>.<setting>` for each name in `Endpoint::setting_names()`; `config_keys()` feeds the loader allow-list so typos are rejected. Each endpoint receives only its own settings under bare names. `configure()` validates every device switch first, then applies the endpoint settings in order without rollback (as Core's `RuntimeManager` does): if one endpoint rejects its settings, earlier endpoints keep theirs and later ones are not configured. The device switches are committed only if the whole call succeeds.
- **Failure during a lifecycle call.** A failing endpoint faults the endpoint and the manager; the endpoint's Error is returned with the manager as `source`.
- **Fail-safe stop.** `stop()` attempts every running endpoint (reverse order) even if one fails, then returns the first error and the manager is FAULT; faulted endpoints are skipped. `shutdown()` is best effort over all endpoints, stops any still-running one, releases faulted endpoints, and returns the first error it met (state unchanged, a retry resumes), so controlled shutdown works after any failure. It covers every registered device, including devices switched off by a configure() after the live period, so no live period is left unreleased.
- **Fault on its own.** An endpoint that faults while the manager is RUNNING leaves the manager RUNNING with UNHEALTHY health and FAILED status, and exactly one ERROR event (source: the manager) goes to the event sink; the endpoint is never restarted. A restart is an explicit new lifecycle: shutdown, then initialize.
- **Sealing and lifetimes.** `initialize()` closes the registry and seals every registered Device (no endpoint can be added afterwards). The manager owns neither Devices nor Endpoints; Devices must outlive it. Its fault listeners are owner-scoped, so several managers can watch one endpoint and each removes only its own.

### Typed data (I3-004)

`samples.hpp` defines one plain value type per endpoint kind, all SI (metres, radians, seconds, device frame): `AccelerationSample` (m/s^2), `AngularVelocitySample` (rad/s) and `PositionSample` (rad), each with a 1-based `sequence` (0 = nothing produced) and a MONOTONIC `timestamp`; and `MotorCommand`, a single velocity target in rad/s. A sample is valid iff every number is finite, and `SensorEndpoint::read()` enforces it. I3 endpoints use a deterministic virtual clock (`virtual_timestamp(n)` = n x 1 ms), never a wall clock. `MotorLimits` (inclusive, default [-1, 1]) and `validate()` reject NaN, Inf and out-of-range commands with `INVALID_ARGUMENT`; an implementation validates before acting and a rejected command has no effect. `typed_endpoints.hpp` names the endpoint interfaces (`AccelerationEndpoint`, `AngularVelocityEndpoint`, `PositionEndpoint`, `MotorCommandEndpoint`) and their capability ids. There is no universal sample type. **The `MotorCommand` contract is a physical-actuator contract and requires human review (open).**

### Mock hardware (I3-005)

`hardware/mock/` (library `kritva_hardware_mock`, depends only on `kritva::hardware`) is shared by the tests and the demo. `MockImuDevice` has `acceleration` and `angular_velocity` endpoints and `MockMotorDevice` has `command` and `position` endpoints over one simulated joint. Data is deterministic: a documented formula per sample with virtual timestamps. Every mock endpoint takes a failure schedule from configuration (`fail_after_ops`/`fail_after_writes`: one transient `INTERNAL_ERROR` after N successful operations; `fault_after_ops`/`fault_after_writes`: the endpoint enters FAULT after the Nth) and offers `inject_fault()`, `fail_next_operation()` and `degrade()`. Sensor settings are `start` and `step` (integers); the position endpoint takes `initial`. The motor takes `min_rad_s`/`max_rad_s` (integers within +/-1000); a setting that is absent leaves the current limit exactly as it is, and `set_limits()` sets fractional limits within the same ceiling, only while the endpoint is not operational (limits are configuration-time properties). The motor is fail-safe: commands are validated before they are applied, a rejected or failed write changes nothing, and the joint model velocity itself is set to zero on initialize, stop, shutdown and on entering FAULT (via `on_fault()`), so a faulted motor reads zero, not only through an accessor. The mocks pass the same conformance suite as any endpoint. **The motor mock is the reference for the actuator contract and requires human review (open).**

### Observation and diagnostics (I3-006)

Observation reuses the I2 model; there is no second diagnostics framework. At component level the `DeviceManager` appears in `RuntimeHost::observe()` with its aggregated status, health (with the failing endpoint named in the detail) and statistics. `DeviceManager::diagnostics()` adds the device and endpoint level: identity, enabled flag, lifecycle state, status, health with the failure reason, successful and failed operation counts, the last error and the capability names, in registration order. `describe()` renders that snapshot as deterministic text; all device- or error-supplied text (messages, health details, capability names) has control characters and double quotes replaced (and commas in capability lists), so it can neither forge a line nor close a quoted field, and configuration values are never included. A faulted endpoint stays faulted and refuses operations (counted as failures) until the explicit sequence shutdown, initialize (a stop is invalid on a faulted endpoint); no automatic recovery exists.
