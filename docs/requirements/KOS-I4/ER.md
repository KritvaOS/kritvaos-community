# ER Requirements

See `docs/requirements/KOS-I4/REQUIREMENTS.md` for the normative traceability table.

Family: ER

## Clarifications (planning review)

`EdgeHost` is an I4 service, not a Core Component: it does not instantiate `RuntimeHost`, `RuntimeManager` or `DeviceManager`. It owns or serves the Edge-side Devices, executes the Nexus lifecycle requests (configure, initialize, start, stop, shutdown) on their endpoints in the I3-defined deterministic order using the existing Endpoint lifecycle contracts, seals the served Devices at the first HELLO, serves reads and writes, owns heartbeat/session supervision and generates responses and events. It must not duplicate Core lifecycle semantics.

## I4-004 clarification

Heartbeat transmission and timeout supervision are I4-006: the `EdgeHost` of I4-004 only records a valid heartbeat. The "owns heartbeat/session supervision" sentence above describes the finished I4 service.
