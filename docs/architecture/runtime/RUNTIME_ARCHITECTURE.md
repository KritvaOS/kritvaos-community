# KritvaOS Runtime Architecture

**Initial implementation:** KOS-I2  
**Dependency:** `kritva-core` R1.0

## 1. Boundary

The runtime/application layer sits above `kritva-core`. Core provides stable common contracts; the runtime composes and orchestrates application components.

```text
Kritva Application
       ↓
KOS-I2 Runtime
       ↓
Sensor / Controller / Monitor components
       ↓
kritva-core R1.0
       ↓
Linux host
```

## 2. Runtime Lifecycle

```text
INITIALIZING → READY → RUNNING → STOPPING → STOPPED
```

Invalid transitions shall be rejected or handled deterministically.

## 3. Component Model

Components have identity, lifecycle, health, configuration, dependencies where needed, and observable failures. Dependency ordering is established before normal execution.

## 4. KOS-I2 Demo

The reference application uses three software-only components:

- Sensor — produces simulated data.
- Controller — consumes/processes simulated data.
- Monitor — observes runtime state, health, statistics, and events.

A controlled sensor fault demonstrates fault/health/event handling.

## 5. Explicit Boundary

KOS-I2 does not implement hardware drivers, EtherCAT, ROS2/DDS, STM32, Nexus/Edge transport, PREEMPT_RT, AI, Sense, Motion, Mind, or Skill.

## 6. KOS-I2 Implementation (as built)

| Element | Location | Role |
|---|---|---|
| `kritva::runtime::RuntimeHost` | `runtime/` | Owns a Core `RuntimeManager`; lifecycle, `add_component`, `configure`, `observe`, `failure_report`, `controlled_shutdown`, `run`. Adds no lifecycle, registry or dependency logic of its own. |
| Configuration | `runtime/` | `key=value` loader into Core `Configuration`; typed readers; `RuntimeSettings` (`runtime.name`, `runtime.tick_ms`). |
| Observation | `runtime/` | `RuntimeObservation` snapshot, bounded `EventLog` (Core `IEventSink`), `FailureReport`, `describe()`. |
| Demo | `examples/kritva_demo/` | Sensor, Controller, Monitor and `DemoApplication`; exit code 0 clean, 1 error, 3 failure handled. |

Behaviors to know:

- Core `INITIALIZING` and `STOPPING` are transient: they are visible to components during their hooks but not once an operation returns. The demo prints `READY`, `RUNNING` and `STOPPED`.
- A component that fails itself while the runtime is `RUNNING` does not change Core's runtime state; the runtime stays `RUNNING`. The failure is detected from component observation (`failure_report()`), and the application responds with `controlled_shutdown()`.
- Configuration input is bounded (64 KiB), `runtime.*` keys are validated, applications can supply a key allow-list, and `runtime.name` is restricted to `[A-Za-z0-9_.-]` (1..64 characters).
- `controlled_shutdown()` reports one ERROR per failure: a component that already failed does not produce a second ERROR when it rejects `stop()`.
- There is no recovery in KOS-I2. A failed component stays `FAULT` / `UNHEALTHY` until shutdown.
- Valid configuration is a precondition of operation (RR-CFG-003): `RuntimeHost::initialize()` (and `run()`) fail with `CONFIGURATION_ERROR` until `configure()` has succeeded once. The requirement is sticky across restarts from `STOPPED`.
- Core `Event` carries no state payload or timestamp; a `LIFECYCLE` event does not say which state was reached. Read the state with `observe()`.


## 7. KOS-I3 Extension Boundary

KOS-I3 extends the runtime downward with a hardware-facing Device/Endpoint abstraction.

```text
Kritva Application
       ↓
KOS Runtime
       ↓
Device Registry / Device Manager
       ↓
Device
       ↓
Endpoint
       ↓
Hardware Abstraction
       ↓
Driver / Platform
```

The I3 implementation uses mock hardware only.

### Runtime ownership

As built, a single `DeviceManager` (`hardware/abstraction/`) is the one Core Component through which the runtime reaches Devices and Endpoints; it is registered with the unchanged `RuntimeHost::add_component()`. Devices and Endpoints are not Core Components and there is no `RuntimeHost::add_device()`. The manager runs the Core Component lifecycle and fans each operation out to the endpoints in deterministic order (see `docs/architecture/hardware/DEVICE_ENDPOINT_ARCHITECTURE.md`). An endpoint fault is visible at component level through `RuntimeHost::observe()` and `failure_report()` (manager health UNHEALTHY while its state stays RUNNING) and reported as exactly one ERROR event; `controlled_shutdown()` works after any endpoint failure.

### Registry distinction

The Core runtime component registry and the I3 Device Registry are separate:

| Registry | Owns |
|---|---|
| Core RuntimeManager registry | Runtime/application components |
| I3 Device Registry | Physical/simulated Devices and their Endpoints |

### I3 operation

The reference flow is:

```text
register → discover → configure → initialize → start
    → sensor read / actuator write
    → observe → fault
    → controlled shutdown
```

### I3 exclusions

I3 does not introduce EtherCAT, CAN, SPI/I2C/UART/GPIO drivers, STM32 firmware, Nexus/Edge transport, ROS2/DDS, PREEMPT_RT, or physical robot hardware.

I4 may provide remote/transport-backed Endpoint implementations while preserving the I3 contract.
