# KOS-I4 Demo

Executable: `kritva_nexus_edge_demo`.

Create RuntimeHost + DeviceManager; start EdgeHost with mock IMU/motor; HELLO; discover; configure; initialize/start; read sensor/position; write valid motor command; query diagnostics; verify heartbeat; disconnect; advance >300 ms; verify Edge motor stopped and Nexus RemoteEndpoint FAULT with one ERROR; inject stale/duplicate traffic and verify rejection; verify no auto recovery; shutdown -> initialize -> fresh HELLO/discovery -> start; verify operation; controlled shutdown. No threads, sleeping or wall clock. Exit 0 only if all assertions pass.

## As built (I4-008)

`examples/nexus_edge_demo/` (README.md there): the application library `kritva_nexus_edge_demo_lib`, the executable `kritva_nexus_edge_demo` and the two configurations. It goes only through the production path (RuntimeHost, DeviceManager, RemoteDevice, RemoteEndpoint, RemoteNode, LoopbackLink, SimulatedTransport, EdgeHost, I3, mocks) and never uses the session layer. Tests: `kritva_nexus_edge_demo_system` (in-process), `kritva_nexus_edge_demo_sanity` (the executable, determinism, invalid and misspelled configuration) and `kritva_nexus_edge_demo_hygiene` (a source scan proving that the demo does not bypass the production path). Observation of the Edge over the link (`observe_edge`) is not used: it is deferred.
