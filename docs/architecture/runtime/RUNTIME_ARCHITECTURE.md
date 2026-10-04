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
