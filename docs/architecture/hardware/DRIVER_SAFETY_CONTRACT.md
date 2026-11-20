<!--
Copyright (c) 2026 Kritva Robotics Private Limited.
SPDX-License-Identifier: Apache-2.0

File        : DRIVER_SAFETY_CONTRACT.md
Description : What a real actuator driver must provide before it may use the KOS-I3 and KOS-I4 contracts.

Component   : KritvaOS Hardware Abstraction
Module      : Actuator Safety
Layer       : Documentation

Author      : KritvaOS
Created     : 20-11-2026
-->

# Actuator Driver Safety Contract

This note records what a real actuator driver, and the integrator who deploys it, must provide on top of the KOS-I3 hardware abstraction and the KOS-I4 Nexus/Edge layer. Today only mock devices and a simulated transport exist, so nothing here has been exercised against physical hardware. It approves nothing: the KOS-I3 and KOS-I4 human safety reviews remain independent, open gates.

## What the software already guarantees

- A motor command is validated against the endpoint's limits before anything is applied, and a rejected command has no effect (`validate()` in `samples.hpp`).
- An endpoint that fails to initialize, start or stop is placed in FAULT. `on_fault()` runs first so the hardware is made safe, then the fault listeners run. A faulted endpoint rejects data operations and leaves FAULT only through `shutdown()`.
- The Edge stops every RUNNING actuator endpoint when a new session replaces the old one and when the heartbeat timeout expires. A stop that fails is counted and leaves that endpoint in FAULT.

## What a real driver must provide

1. **A safe state that does not depend on the limits.** `on_fault()`, `on_stop()` and `on_shutdown()` must drive the output to its safe value directly, as the mock motor does, and must not go through command validation. The configured limits must never be able to block the safe state.
2. **A report of failure to reach the safe state.** `on_fault()` returns `void` and must not throw, so it cannot report failure. A driver that cannot confirm the safe state must also fail `on_stop()` (so the endpoint faults and is counted) and must expose the condition through its health detail. Reaching the safe state is not assumed from a successful return.
3. **A bounded stop time.** `stop()` runs in the caller's thread and has no timeout of its own. A driver must bound the time it spends reaching the safe state, or the integrator must run the Edge where a blocked stop cannot delay supervision of other endpoints.
4. **Idempotent safe-state handling.** The Edge may stop an endpoint on a new session and again on a timeout. Repeating the safe-state action must be harmless.
5. **Limits that match the hardware.** The mock default of [-1, 1] rad/s and the 1000 rad/s ceiling are software values. A driver must supply limits derived from the real joint and drive, and must reject a configuration it cannot honour.

## What the integrator must provide

1. **A supervision loop.** The Edge checks the heartbeat timeout only inside `EdgeHost::poll()`, against the transport's clock. The integrator must call `poll()` often enough that the timeout is honoured, and must treat a stalled loop as a safety fault in its own right.
2. **A timeout ceiling.** Protocol 1.0 allows a heartbeat timeout of up to 600000 ms, during which a silent peer leaves an actuator at its last command. For a physical system the integrator should call `EdgeHost::set_max_heartbeat_timeout_ms()` with a ceiling suited to the machine. A HELLO that asks for more is rejected.
3. **Handling of a lost FAULT_EVENT.** The Edge drops a FAULT_EVENT when no session exists. The endpoint's own state is authoritative, so the Nexus must read endpoint state after reconnecting and must not rely on having received the event.
4. **Its own review.** The KOS-I3 and KOS-I4 reviews cover the software contract only. A physical deployment needs a separate safety assessment of the machine.
