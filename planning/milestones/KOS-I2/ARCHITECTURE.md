# KOS-I2 Architecture

```text
                  Kritva Application
                         │
                         ▼
                  KOS-I2 Runtime
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
          Sensor      Controller   Monitor
             └───────────┼───────────┘
                         ▼
                  kritva-core R1.0
                         ▼
                     Linux Host
```

The runtime owns composition, lifecycle orchestration, configuration, observation, and controlled application failure behavior. Core remains the stable contract/foundation layer.
