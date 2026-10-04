# KOS-I2 Changelog

## Unreleased

- Initial KOS-I2 planning structure.
- Defined runtime/application foundation scope.
- I2-001: added RuntimeHost (runtime/), kritva_demo entry point (examples/kritva_demo/), and runtime unit/integration/sanity tests (tests/runtime/).
- I2-002: added RuntimeHost::add_component and composition unit/integration/sanity tests.
- I2-003: added key=value configuration loader, typed validation, RuntimeHost::configure, demo optional config-file argument and tests.
- I2-004: added RuntimeHost::observe, EventLog, host runtime events, describe() diagnostics and tests.
- I2-005: added FailureReport/failure_report(), controlled_shutdown() (also used by run() cleanup) and failure tests.
- I2-006: added Sensor, Controller, Monitor, DemoApplication, demo configs (normal and failure), and unit/system/sanity tests.
- Audit follow-up: demo tests guarded for KRITVA_BUILD_EXAMPLES=OFF, tightened test ordering checks, documented application-level configuration rule, added docs/verification/KOS-I2_VERIFICATION.md.
- RuntimeHost::initialize()/run() now require a prior successful configure() (RR-CFG-003); tests updated (ProbeComponent no longer logs configure by default).
- Hardening: unknown-key rejection, 64 KiB configuration limit, runtime.name validation, no duplicate shutdown ERROR event, stronger no-recovery test.
- CI: added a build-and-test job (configure, build, ctest in the kritvaos-dev:0.1 container, plus the source header check).
- Added mandatory unit, integration, sanity, regression, review, and commit gates.
