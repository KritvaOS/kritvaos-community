# Planning Roadmap

| Milestone | Focus | Status |
|---|---|---|
| KOS-I2 | Runtime / Application Foundation | Implemented |
| KOS-I3 | Device & Endpoint Integration | Implemented; human safety review OPEN |
| KOS-I4 | Nexus ↔ Edge Integration | Planned |
| KOS-I5 | Sense / Motion Integration | Future |
| KOS-I6 | First Physical Robot Demonstration | Future |

## KOS-I3 → I4 Boundary
I3 defines the stable Device/Endpoint abstraction.
I4 defines how Endpoints are connected across Nexus/Edge and transport boundaries.
Do not pull I4 transport concerns into I3.

I3 software closure does not close its independent human safety review gate.
