# KOS-I3 Changelog

## Implemented

- I3-001: Device/Endpoint contracts (`hardware/abstraction/`, library `kritva_hardware`): identity, `Endpoint` lifecycle over Core `Lifecycle`, `SensorEndpoint<T>`, `ActuatorEndpoint<T>`, `Device`, reusable conformance suite and tests.

- I3-002: DeviceRegistry (non-owning, registration-order enumeration, id and name lookup, endpoint lookup, duplicate rejection, close) and tests.

- I3-003: DeviceManager (one Core Component; deterministic fan-out, scoped configuration, endpoint fault reported once, controlled shutdown after failure), plus additive Endpoint setting_names() and fault listener.

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
