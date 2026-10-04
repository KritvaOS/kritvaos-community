# KOS-I2 Demo Plan

## Normal run

1. Load configuration.
2. Register Sensor, Controller, Monitor.
3. Initialize components.
4. Reach READY.
5. Start and reach RUNNING.
6. Observe status, health, events, statistics.
7. Run simulated application activity.
8. Stop cleanly and reach STOPPED.

## Failure run

1. Start normally.
2. Inject Sensor failure.
3. Verify Sensor fault/unhealthy state.
4. Verify failure event.
5. Verify Monitor observes failure.
6. Perform controlled shutdown.
