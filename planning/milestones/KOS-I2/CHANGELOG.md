# KOS-I2 Changelog

## Unreleased

- Initial KOS-I2 planning structure.
- Defined runtime/application foundation scope.
- I2-001: added RuntimeHost (runtime/), kritva_demo entry point (examples/kritva_demo/), and runtime unit/integration/sanity tests (tests/runtime/).
- I2-002: added RuntimeHost::add_component and composition unit/integration/sanity tests.
- I2-003: added key=value configuration loader, typed validation, RuntimeHost::configure, demo --config support and tests.
- I2-004: added RuntimeHost::observe, EventLog, host runtime events, describe() diagnostics and tests.
- I2-005: added FailureReport/failure_report(), controlled_shutdown() (also used by run() cleanup) and failure tests.
- Added mandatory unit, integration, sanity, regression, review, and commit gates.
