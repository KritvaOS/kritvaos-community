# Planning Decisions

- Core R1.0 is treated as a stable dependency for KOS-I2.
- KOS-I2 is Linux-host software only.
- Runtime abstractions remain minimal and evidence-driven.
- Hardware/distributed functionality is deferred to later milestones.

## KOS-I3 implementation alignment (Claude and ChatGPT, 2026-10-04)

Approved before I3-001; these supersede any conflicting wording in the I3 planning text.

| # | Decision |
|---|---|
| 1 | One `DeviceManager` is a single Core `Component` registered with the existing `RuntimeHost::add_component()`. `RuntimeHost`, `RuntimeManager` and Core are unchanged; there is no `add_device()`. Devices and Endpoints are not Components (DER-003). It fans out configure/initialize/start in deterministic order and stop/shutdown in reverse. |
| 2 | Endpoints reuse the released Core `Lifecycle` and the Component operation table (configure/initialize from UNKNOWN or STOPPED, start from READY, stop from READY or RUNNING, FAULT left only by shutdown). No new state enum or state machine (DER-407). |
| 3 | An endpoint fault makes that endpoint FAULT/UNHEALTHY; the `DeviceManager` stays RUNNING but reports Health UNHEALTHY and emits exactly one ERROR event. `stop()` stops healthy endpoints; `shutdown()` releases the faulted one. No automatic recovery. |
| 4 | One typed sample per endpoint (`AccelerationSample`, `AngularVelocitySample`, `PositionSample`, `MotorCommand`) on a small `Vec3` with documented SI units; templates `SensorEndpoint<Sample>` (`read(Sample&)`) and `ActuatorEndpoint<Command>` (`write(const Command&)`). No universal variant. This replaces the single `ImuSample` / `EncoderSample` wording. |
| 5 | `DeviceId` / `EndpointId` are strong types over `core::Id` (non-zero) plus a required unique non-empty name; lookup by id and by name; endpoint ids unique within a Device. |
| 6 | The integrator owns Devices; the registry holds non-owning pointers; a Device owns its Endpoints (valid for the Device lifetime); registration closes at the first `initialize()`. |
| 7 | Mocks live in a reusable library `kritva_hardware_mock` under `hardware/mock/` (depends only on `hardware/abstraction`), used by the tests and by `examples/device_demo`. Not under `tests/`. |
| 8 | Released Core `ErrorCode` only: not RUNNING = `NOT_READY`; FAULT endpoint = `RESOURCE_UNAVAILABLE`; injected hardware failure = `INTERNAL_ERROR`; invalid argument or out-of-range command = `INVALID_ARGUMENT`; invalid lifecycle call = `INVALID_STATE`. `Error.source` is the `DeviceManager` component id; device and endpoint identity are carried in endpoint diagnostics. |
| 9 | Samples carry a sequence number and a deterministic virtual timestamp from a per-mock counter. No wall clock. |
| 10 | `MotorCommand` is a single velocity target (rad/s), no mode enum. The mock enforces configured limits and rejects NaN/Inf and out-of-range values. I3-004 and I3-005 require human review (AGENTS.md section 32, physical actuator control). |
| 11 | I3-001 delivers public headers, documented contracts and a reusable conformance suite exercised against a minimal test double. No mocks until I3-005. |
| 12 | Settings use Core `Configuration` keys `<device>.<endpoint>.<setting>` and `<device>.enabled`; `DeviceManager::config_keys()` feeds the I2 loader allow-list. Minimal settings: sensor sequence start/step and `fail_after_ops`; motor min/max limits and `fail_after_writes`. |
| 13 | `MANIFEST.json` was a packaging artifact referenced by nothing; removed. |
| 14 | Strictly sequential I3-001 to I3-007; one atomic commit per task (`KOS-I3: implement <description>`); evidence and hash recorded in the planning files; the full I2 regression plus I3 tests plus the CI job are the gate each time. |
