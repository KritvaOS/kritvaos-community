# KritvaOS Device and Endpoint Requirements

**Applies to:** KOS-I3 Device & Endpoint Integration  
**Dependency:** `kritva-core` R1.0  
**Status:** DRAFT — KOS-I3 planning baseline

## 1. Purpose

Define the stable hardware-facing Device and Endpoint requirements introduced by KOS-I3.

These requirements describe **what** a Device and Endpoint provide. They do not prescribe EtherCAT, CAN, SPI, I2C, UART, MCU firmware, Nexus/Edge transport, or a particular hardware implementation.

## 2. Architectural Principles

- **DER-001:** Device/Endpoint APIs shall remain transport-independent.
- **DER-002:** Device/Endpoint shall not duplicate Core status, health, result, or error contracts.
- **DER-003:** Device/Endpoint shall not become Core runtime components merely to obtain lifecycle behavior.
- **DER-004:** Runtime shall coordinate Device/Endpoint operation.
- **DER-005:** Physical driver implementations shall remain below the hardware abstraction.
- **DER-006:** Simulation/mocks shall use the same contracts as physical implementations where practical.

## 3. Identity and Discovery

- **DER-101:** Every Device shall have a stable unique identifier within its registry.
- **DER-102:** Every Endpoint shall have a stable identifier within its owning Device.
- **DER-103:** Device registration shall reject duplicate identifiers deterministically.
- **DER-104:** Endpoint registration shall reject duplicate identifiers deterministically.
- **DER-105:** Device lookup shall be deterministic.
- **DER-106:** Endpoint lookup shall be deterministic.
- **DER-107:** Missing Device/Endpoint lookup shall return an explicit failure.
- **DER-108:** Registry enumeration shall be deterministic.

## 4. Device Contract

- **DER-201:** A Device shall represent a physical or simulated physical device.
- **DER-202:** A Device shall expose identity.
- **DER-203:** A Device shall expose its contained Endpoints.
- **DER-204:** A Device shall expose capability metadata sufficient for discovery.
- **DER-205:** Device configuration shall be explicit and validated.
- **DER-206:** Device ownership and lifetime shall be unambiguous.

## 5. Endpoint Contract

- **DER-301:** An Endpoint shall represent one physical capability/interface of a Device.
- **DER-302:** An Endpoint shall expose identity.
- **DER-303:** An Endpoint shall expose capability/type information.
- **DER-304:** An Endpoint shall expose status using the approved Core contract.
- **DER-305:** An Endpoint shall expose health using the approved Core contract.
- **DER-306:** Endpoint operation shall have deterministic error semantics.
- **DER-307:** An Endpoint shall not embed a transport-specific implementation contract.

## 6. Lifecycle

- **DER-401:** Device/Endpoint initialization shall be explicit.
- **DER-402:** Device/Endpoint start shall be explicit.
- **DER-403:** Device/Endpoint stop shall be explicit.
- **DER-404:** Invalid lifecycle operations shall fail deterministically.
- **DER-405:** Runtime startup shall use deterministic Device/Endpoint ordering.
- **DER-406:** Runtime shutdown shall use deterministic reverse/ownership-compatible ordering.
- **DER-407:** I3 shall not create a competing global lifecycle state machine.

## 7. Sensor and Actuator Operations

- **DER-501:** Sensor operations shall use typed sample contracts.
- **DER-502:** Actuator operations shall use typed command contracts.
- **DER-503:** Sensor read shall fail explicitly when the endpoint is not operational.
- **DER-504:** Actuator write shall fail explicitly when the endpoint is not operational.
- **DER-505:** Sample/command units and validity semantics shall be documented.
- **DER-506:** I3 shall not require real-time execution guarantees.
- **DER-507:** I3 shall not introduce one universal variant containing all sensor/actuator data types.

## 8. Observation and Diagnostics

- **DER-601:** Device status shall be observable.
- **DER-602:** Endpoint status shall be observable.
- **DER-603:** Device health shall be observable.
- **DER-604:** Endpoint health shall be observable.
- **DER-605:** Capabilities shall be observable.
- **DER-606:** Relevant operation statistics shall be observable.
- **DER-607:** Endpoint failure shall be observable.
- **DER-608:** Diagnostics shall not intentionally expose secrets.

## 9. Fault Handling

- **DER-701:** Endpoint faults shall be represented explicitly.
- **DER-702:** Injected/mock faults shall be deterministic.
- **DER-703:** Faulted endpoints shall not silently recover.
- **DER-704:** Controlled runtime shutdown shall remain possible after endpoint failure.
- **DER-705:** Automatic recovery is not required by I3.
- **DER-706:** If a future recovery API is introduced, it shall be specified separately rather than implied by I3.

## 10. Testability

- **DER-801:** Device/Endpoint contracts shall be unit-testable without physical hardware.
- **DER-802:** A mock Device/Endpoint implementation shall exist for I3.
- **DER-803:** Nominal and failure paths shall be tested.
- **DER-804:** The reference demo shall provide an end-to-end system test.
- **DER-805:** All I3 tests shall be reproducible on the supported Linux development environment.

## 11. I3/I4 Boundary

- **DER-901:** I3 shall define Endpoint semantics independently of transport.
- **DER-902:** I4 may introduce remote/transport-backed Endpoint implementations.
- **DER-903:** I3 shall not require Nexus/Edge communication.
- **DER-904:** I3 shall not require EtherCAT or another fieldbus.
- **DER-905:** A future remote Endpoint shall preserve the stable I3 contract where practical.

## 12. Traceability

| Requirement group | Primary task |
|---|---|
| DER-001..006 | I3-001 |
| DER-101..108 | I3-002 |
| DER-201..206 | I3-001, I3-003 |
| DER-301..307 | I3-001 |
| DER-401..407 | I3-003 |
| DER-501..507 | I3-004 |
| DER-601..608 | I3-006 |
| DER-701..706 | I3-006 |
| DER-801..805 | I3-005, I3-007 |
| DER-901..905 | I3-001, I3-007 |
