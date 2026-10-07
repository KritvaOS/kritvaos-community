# KOS-I4 Nexus-Edge reference demo

`kritva_nexus_edge_demo [config-file]` runs a Nexus and an Edge in one process, from the `RuntimeHost` to mock hardware, and checks
every step. It is software only: Linux host, simulation, no hardware, no wall clock, no threads, no sleeping. The same
configuration always produces the same output. Exit code 0 only if every expectation held, 1 otherwise.

```text
Application -> RuntimeHost -> DeviceManager -> RemoteDevice -> RemoteEndpoint -> RemoteNode -> LoopbackLink
            -> SimulatedTransport -> EdgeHost -> I3 Device / Endpoint -> mock hardware
```

The application finds its endpoints in the `DeviceManager` and uses the unchanged I3 interfaces (`AccelerationEndpoint`,
`MotorCommandEndpoint`, ...). It never uses the session layer, never drives supervision itself (the `LoopbackLink` is the one
explicit driver) and never controls the Edge's hardware directly; `tests/hardware/unit/nexus_edge_demo_hygiene_test.cpp` enforces
that on the source.

## Scenario

1. Compose the Edge (mock IMU and motor behind an `EdgeHost`) and the Nexus; read the link timing from the configuration (it is
   proposed in HELLO); connect and discover; validate the whole configuration against the allow-list; configure the runtime.
2. `initialize` and `start` through the `RuntimeHost` (a fresh session).
3. Read the sensors and the position, write a motor command (the Edge validates it against its own limits and refuses an
   out-of-limit one), observe through the runtime and the passive diagnostics of both sides.
4. Run on with both sides driven: heartbeats in both directions.
5. `demo.hostile_link`: duplicated and replayed frames. A duplicated write is applied once; a replayed older write is dropped as
   stale and never applied; duplicated answers are rejected.
6. Lose the link. The Nexus faults every live remote endpoint (reason `link lost`, one ERROR each, one link record). The Edge's
   actuator keeps running until its own heartbeat timeout, then the Edge stops it through the I3 `stop()`; its sensors keep
   running. The two views differ until both are driven, and nothing hides that.
7. Nothing recovers by itself: 2 more seconds, observation and diagnostics change nothing and send nothing.
8. Recover explicitly through the runtime: `stop`, `shutdown`, `initialize` (a fresh HELLO and discovery), `start`. A second live
   period works; the faults are cleared only by that lifecycle.
9. Controlled shutdown: everything STOPPED.

## Configuration

`runtime.name` is required. `link.heartbeat_period_ms`, `link.heartbeat_timeout_ms`, `link.request_timeout_ms`,
`link.pump_quantum_ms` set the link timing. `demo.link_latency_ms` (0..50), `demo.command_milli_rad_s`, `demo.run_ms` (10..5000),
`demo.hostile_link` shape the scenario. `<device>.enabled` and the declared remote settings
(`<device>.<endpoint>.<setting>`) are sent to the Edge endpoints, which validate and own them. A misspelled key or an invalid value
is rejected before anything runs. See `nexus_edge_demo.conf` and `nexus_edge_demo_clean.conf`.

## Safety

The Edge is the actuator-safety authority; the Nexus supervision only keeps its own representation correct. **The human safety
review of the KOS-I3 and KOS-I4 actuator paths is OPEN.** This demo is software evidence in simulation. It does not close that
review and does not make anything fit for a physical actuator.
