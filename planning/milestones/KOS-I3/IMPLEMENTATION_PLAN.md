# KOS-I3 Implementation Plan

**Status:** IMPLEMENTED (as built). This document records the architecture that was approved (`planning/DECISIONS.md`, section "KOS-I3 implementation alignment") and implemented in the seven task commits, the audit remediation and the re-audit follow-up. Evidence is in `docs/verification/KOS-I3_VERIFICATION.md`. The human safety review of the actuator contract is **OPEN** (section 12).

## 1. Strategy

I3 extends the KOS-I2 runtime downward toward hardware without changing Core R1.0, RuntimeHost, RuntimeManager or any I2 code. Tasks were implemented strictly in order I3-001 to I3-007, one atomic commit each.

```text
hardware/
├── abstraction/            library kritva_hardware (depends only on kritva_core)
│   ├── include/kritva/hardware/
│   ├── src/
│   └── CMakeLists.txt
└── mock/                   library kritva_hardware_mock (depends only on kritva::hardware)
    ├── include/kritva/hardware/mock/
    ├── src/
    └── CMakeLists.txt
tests/hardware/             unit, integration, sanity, conformance, support
examples/device_demo/       reference application (kritva_device_demo)
```

Mocks are a reusable library shared by the tests and the demo; they are not under `tests/`.

## 2. Contract boundary (as built)

```text
Application
   ↓
RuntimeHost  (unchanged; RuntimeHost::add_component)
   ↓
DeviceManager   ← the one hardware-facing Core Component
   ↓
DeviceRegistry  (non-owning, registration order)
   ↓
Device          (owns its Endpoints; not a Component; no lifecycle of its own)
   ↓
Endpoint        (Core Lifecycle; typed SensorEndpoint<T> / ActuatorEndpoint<T>)
   ↓
Mock hardware   (I3)   |   Nexus ↔ Edge transport, real drivers (I4 and later)
```

- Devices and Endpoints are not Core Components (compile-time assertions in the tests); there is no `RuntimeHost::add_device()`.
- Core status, health, result, error, capability, statistics, event and lifecycle types are reused; none is duplicated.
- Errors use released Core `ErrorCode`s only: not RUNNING `NOT_READY`; on a FAULT endpoint `RESOURCE_UNAVAILABLE`; injected hardware failure `INTERNAL_ERROR`; invalid argument `INVALID_ARGUMENT`; invalid lifecycle call `INVALID_STATE`. Endpoints leave `Error::source` unset; the `DeviceManager` sets its component id.
- Ownership: the integrator owns Devices (the registry and manager hold non-owning pointers and Devices must outlive the manager); a Device owns its Endpoints.

## 3. I3-001 — Device / Endpoint contracts (`39780cc`)

`DeviceId` / `EndpointId` strong types over Core `Id` semantics with a unique name of 1..64 characters of `[a-z0-9_]`; `Endpoint` implementing the Core Component lifecycle table on a Core `Lifecycle` (no new state machine); `SensorEndpoint<Sample>` (`read`), `ActuatorEndpoint<Command>` (`write`); `Device` (identity, owned endpoints, aggregated status, health and capabilities); a reusable endpoint conformance suite that every implementation, including the mocks, passes unchanged.

## 4. I3-002 — Registry (`a7e08e5`)

`DeviceRegistry`: registration order enumeration, duplicate id/name rejection, lookup by id or name, endpoint lookup by Device and Endpoint (id or names), explicit `INVALID_ARGUMENT` for a missing device or endpoint, `close()` ends registration, no unregistration. Distinct from the Core component registry.

## 5. I3-003 — DeviceManager (`97ab518`, hardened in the remediation)

The single Core Component. Fans configure/initialize/start out in registration order and stop/shutdown in the exact reverse. Configuration keys `<device>.enabled` and `<device>.<endpoint>.<setting>` (`Endpoint::setting_names()`, `config_keys()` for the loader allow-list); each endpoint receives only its own settings. `configure()` validates all switches first, applies endpoint settings in order without rollback (as Core does) and commits the switches only on success. `initialize()` closes the registry and seals every registered Device (`Device::seal()`). `stop()` attempts every running endpoint (fail-safe, best effort) and returns the first error; `shutdown()` is best effort over all registered endpoints and returns the first error (state unchanged, a retry resumes). An endpoint that faults by itself leaves the manager RUNNING with UNHEALTHY health and reports exactly one ERROR event; the endpoint is never restarted. Endpoint fault handling uses an `on_fault()` hook (called before the listeners, once per fault) and owner-scoped fault listeners; a hook that faults the endpoint fails the operation without a second notification. Failed `configure`/lifecycle calls return the endpoint's Error with the manager as source.

## 6. I3-004 — Typed data (`f980675`)

One typed sample per endpoint, all SI and trivially copyable: `AccelerationSample`, `AngularVelocitySample`, `PositionSample` (each with a 1-based sequence and a MONOTONIC timestamp from a deterministic virtual clock) and `MotorCommand` (a single velocity target in rad/s). Named interfaces `AccelerationEndpoint`, `AngularVelocityEndpoint`, `PositionEndpoint`, `MotorCommandEndpoint`; capability ids; `MotorLimits` and `validate()` (NaN, Inf and out-of-range rejected with `INVALID_ARGUMENT`, validate before act). `SensorEndpoint::read()` enforces `is_valid()` on a temporary and writes the caller's sample only on success. No universal sample type. Not real-time.

## 7. I3-005 — Mocks (`cd1a35c`, hardened in the remediation)

`MockImuDevice` (acceleration, angular_velocity) and `MockMotorDevice` (command, position over one simulated joint). Deterministic data from documented formulas; configuration-driven transient failure and fault after N operations (`fail_after_*`, `fault_after_*`); programmatic `inject_fault()`, `fail_next_operation()`, `degrade()`. The motor is fail-safe: validate before apply, a rejected or failed write changes nothing, the model velocity is zeroed on initialize, stop, shutdown and on entering FAULT; limits are lossless, within +/-1000, and frozen once the endpoint is operational.

## 8. I3-006 — Observation, diagnostics, fault handling (`46b91fa`, hardened in the remediation)

Observation reuses the I2 model (`ComponentObservation`, `IComponentStatistics`, `EventLog`, `failure_report()`). `DeviceManager::diagnostics()` and a deterministic `describe()` give device and endpoint identity, state, status, health with the failure reason, operation counts, last error and capability names; all device- or error-supplied text is sanitised (control characters, double quotes, and separators in the unquoted capability lists), and configuration values are never included. A faulted endpoint stays faulted and refuses operations until the explicit sequence shutdown, initialize; there is no automatic recovery.

## 9. I3-007 — Reference demo (`d8cd315`)

`examples/device_demo` (`kritva_device_demo`): a RuntimeHost with one DeviceManager over the mock IMU and motor. Sequence: configure, register, discover every endpoint by name, initialize, start, read the IMU and the joint position, write a motor command, print status, health, capabilities and statistics, inject an endpoint fault, verify it is observed, verify no silent recovery and that healthy endpoints keep working, controlled shutdown, verify STOPPED. Exit 0 when the scenario behaves as expected, 1 on any error. Deterministic: no wall clock, threads or sleeping.

## 10. Verification gate (every task)

```text
Build → Unit → Integration → Sanity → Regression (full KOS-I2 + I3) → Review → Atomic commit
```

Each task also recorded mutation checks; I3-003 onward ran the new tests under ASan and UBSan. CI runs configure, build and the full `ctest` in the project container.

## 11. Restrictions (held)

No `core/` change; no EtherCAT, CAN, ROS2/DDS, MCU firmware, PREEMPT_RT, Nexus/Edge transport, vendor SDK or physical driver; no transport abstraction needed only by I4; no universal sample type; no duplicated Core types; no I2 code moved.

## 12. Human safety review (OPEN)

The `MotorCommand` contract, motor limits, fault-to-zero behavior, velocity command semantics, the motor mock's safety behavior and any future mapping to physical actuator control require human review (AGENTS.md section 32). The software audit and re-audit do not close this gate.
