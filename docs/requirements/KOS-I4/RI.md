# RI Requirements

See `docs/requirements/KOS-I4/REQUIREMENTS.md` for the normative traceability table.

Family: RI

## Clarifications (planning review)

The Nexus side reuses the existing `DeviceManager`, which registers `RemoteDevice`s; there is no `RemoteDeviceManager`. `RuntimeHost`, `RuntimeManager` and Core are unchanged. `EdgeHost` is not a Core Component and there is no Edge `RuntimeHost` or `DeviceManager` in I4.
