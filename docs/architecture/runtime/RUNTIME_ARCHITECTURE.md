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
- There is no recovery in KOS-I2. A failed component stays `FAULT` / `UNHEALTHY` until shutdown.
- Valid configuration is a precondition of operation (RR-CFG-003): `RuntimeHost::initialize()` (and `run()`) fail with `CONFIGURATION_ERROR` until `configure()` has succeeded once. The requirement is sticky across restarts from `STOPPED`.
- Core `Event` carries no state payload or timestamp; a `LIFECYCLE` event does not say which state was reached. Read the state with `observe()`.
