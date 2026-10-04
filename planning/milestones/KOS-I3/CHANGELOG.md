# KOS-I3 Changelog

## Implemented

- I3-001: Device/Endpoint contracts (`hardware/abstraction/`, library `kritva_hardware`): identity, `Endpoint` lifecycle over Core `Lifecycle`, `SensorEndpoint<T>`, `ActuatorEndpoint<T>`, `Device`, reusable conformance suite and tests.

- I3-002: DeviceRegistry (non-owning, registration-order enumeration, id and name lookup, endpoint lookup, duplicate rejection, close) and tests.

- I3-003: DeviceManager (one Core Component; deterministic fan-out, scoped configuration, endpoint fault reported once, controlled shutdown after failure), plus additive Endpoint setting_names() and fault listener.

- I3-004: typed data contracts: Vec3 and per-endpoint samples with SI units, validity, default state and virtual-timestamp policy; MotorCommand with limits and validation; typed endpoint aliases and capability descriptions.

- I3-005: mock devices in hardware/mock (library kritva_hardware_mock): MockImuDevice, MockMotorDevice, deterministic data, scheduled failure/fault injection, degradation, fail-safe motor, and tests.

- I3-006: DeviceManager::diagnostics() (device and endpoint snapshots with status, health, capabilities, statistics, last error and failure reason) and a deterministic, line-safe describe() text; fault evidence tests.

- I3-007: reference device demo (examples/device_demo, kritva_device_demo): configure, register, discover, initialize, start, read IMU, write motor command, observe, inject endpoint fault, verify no silent recovery, controlled shutdown; in-process and end-to-end tests.

## Audit remediation (one follow-up commit)

- Restored `DEVICE_ENDPOINT_ARCHITECTURE.md` (truncated in 97ab518) and re-added the I3-003..I3-006 sections.
- `DeviceManager::stop()` stops every running endpoint (best effort) and returns the first error; `shutdown()` returns the first stop/shutdown error; `configure()` validates switches first and commits them only on success (no rollback of endpoint settings, as in Core).
- `Device::seal()` at initialize; owner-scoped fault listeners; `Endpoint::on_fault()` hook; hook-induced faults no longer double-notify.
- `SensorEndpoint::read()` enforces sample validity and leaves the caller's sample unchanged on failure.
- Motor mock: lossless limits, ceiling, frozen once operational, model velocity zeroed on FAULT.
- Diagnostics sanitise quotes and capability names.
- Tests added for each finding; honesty fixes to the I3-007 acceptance and the verification report.

## Re-audit follow-up

- `DeviceManager::shutdown()` releases the endpoints of devices that were switched off after the live period; capability names in diagnostics cannot forge tokens; the DER-608 guard test is restored; recovery wording corrected.

## Planned

- Define Device and Endpoint hardware abstraction contracts.
- Add deterministic Device/Endpoint registry.
- Integrate endpoint lifecycle with the KOS-I2 runtime.
- Add typed sensor and actuator data interfaces.
- Add mock devices and endpoints.
- Add endpoint diagnostics and fault injection.
- Add reference Device/Endpoint demo.
- Extend verification and regression strategy.

## Boundary

KOS-I3 does not modify `kritva-core` R1.0 and does not introduce EtherCAT, CAN, ROS2/DDS, MCU firmware, Nexus/Edge transport, or physical drivers.
