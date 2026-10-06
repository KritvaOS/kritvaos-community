# RI Requirements

See `docs/requirements/KOS-I4/REQUIREMENTS.md` for the normative traceability table.

Family: RI

## Clarifications (planning review)

The Nexus side reuses the existing `DeviceManager`, which registers `RemoteDevice`s; there is no `RemoteDeviceManager`. `RuntimeHost`, `RuntimeManager` and Core are unchanged. `EdgeHost` is not a Core Component and there is no Edge `RuntimeHost` or `DeviceManager` in I4.

## I4-007 clarification

RI-001..003 are met by composition, not by new framework: `RuntimeHost` hosts the existing `DeviceManager`, which registers the `RemoteDevice`s; no file of `runtime/`, `core/`, `hardware/abstraction`, `hardware/mock` or `hardware/transport` changed. The only supervision entry points are `RemoteNode::service()` and `EdgeHost::poll()`; `LoopbackLink` calls them explicitly and nothing else does. The I4-008 demo must go through `RuntimeHost`, `DeviceManager`, `RemoteDevice`, `RemoteEndpoint` and `LoopbackLink`, never directly through `RemoteSession`.
