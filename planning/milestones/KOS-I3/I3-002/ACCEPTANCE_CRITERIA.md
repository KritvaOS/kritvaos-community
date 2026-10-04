# I3-002 Acceptance Criteria

## Functional
- [x] Device registry implemented.
- [x] Device registration implemented.
- [x] Device lookup implemented.
- [x] Endpoint registration implemented.
- [x] Endpoint lookup implemented.
- [x] Duplicate Device IDs rejected.
- [x] Duplicate Endpoint IDs rejected within a Device.
- [x] Missing Device/Endpoint lookup fails deterministically.
- [x] Enumeration order is deterministic.
- [x] Registry ownership/lifetime is documented.

## Unit
- [x] Registration tests PASS.
- [x] Duplicate tests PASS.
- [x] Lookup tests PASS.
- [x] Missing lookup tests PASS.
- [x] Enumeration tests PASS.

## Integration
- [x] Runtime can discover registered devices/endpoints.

## Sanity
- [x] Clean build PASS.
- [x] Basic register/discover scenario PASS.

## Regression
- [x] All KOS-I2 tests PASS.
- [x] No unexpected regression.

## Review
- [x] Registry is separate from Core RuntimeManager registry.
- [x] No speculative service-discovery framework.
- [x] Error semantics use existing Core/result conventions where applicable.

## Git
- [x] Diff reviewed.
- [x] Atomic commit.
- [ ] Commit hash recorded after commit.

## Evidence (I3-002)

- Build: Debug, 0 warnings from KOS-I3 code.
- Unit: `kritva_hardware_registry_unit` PASS (empty registry, find by id and name, endpoint lookup by id and name with the same endpoint id in two devices, exact missing-lookup errors, duplicate id/name/same device rejected, registration-order enumeration, closed registry).
- Integration: `kritva_hardware_registry_integration` PASS (discovery by name and id reaches the same typed endpoints, which are then operated through the common contracts).
- Sanity: `kritva_hardware_header_sanity` PASS (now includes `device_registry.hpp`); the abstraction still links only `kritva_core`.
- Regression: `ctest` 101/101 PASS (76 Core + 18 I2 + 7 I3). `make check` PASS. `git diff --check` clean.
- Mutation checks (duplicate name, closed registry, enumeration order) each fail the tests.
- Core: unchanged. The registry is separate from the Core component registry; it does not own devices and has no unregistration (registration closes at the first initialize, decision 6). Missing lookups use `INVALID_ARGUMENT` because Core R1.0 has no NOT_FOUND code.
