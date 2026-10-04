# KOS-I4 Demo

Executable: `kritva_nexus_edge_demo`.

Create RuntimeHost + DeviceManager; start EdgeHost with mock IMU/motor; HELLO; discover; configure; initialize/start; read sensor/position; write valid motor command; query diagnostics; verify heartbeat; disconnect; advance >300 ms; verify Edge motor stopped and Nexus RemoteEndpoint FAULT with one ERROR; inject stale/duplicate traffic and verify rejection; verify no auto recovery; shutdown -> initialize -> fresh HELLO/discovery -> start; verify operation; controlled shutdown. No threads, sleeping or wall clock. Exit 0 only if all assertions pass.
