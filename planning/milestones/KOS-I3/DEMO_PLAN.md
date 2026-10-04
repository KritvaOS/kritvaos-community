# KOS-I3 Demo Plan

## Objective

Demonstrate a complete device/endpoint interaction using mock hardware while proving the I3 architectural boundary.

## Topology

```text
Runtime
 ├── left_arm_imu
 │    ├── acceleration
 │    └── angular_velocity
 │
 └── shoulder_motor
      ├── command
      └── position
```

## Demo Steps

1. Load valid runtime configuration.
2. Construct RuntimeHost.
3. Create Device Registry.
4. Register IMU Device.
5. Register motor Device.
6. Discover acceleration, angular velocity, command, and position endpoints.
7. Initialize devices/endpoints.
8. Start devices/endpoints.
9. Read deterministic IMU samples.
10. Write a deterministic motor command.
11. Print status, health, capabilities, and statistics.
12. Inject an endpoint read/write fault.
13. Verify fault and health observation.
14. Verify no silent recovery.
15. Execute controlled shutdown.
16. Verify STOPPED/clean exit.

## Expected Result

The demo exits successfully after the controlled fault/shutdown scenario, with deterministic observable output and no physical hardware dependency.
