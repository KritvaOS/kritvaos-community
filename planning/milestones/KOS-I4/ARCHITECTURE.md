# KOS-I4 Architecture

```text
Application
  -> RuntimeHost
  -> DeviceManager
     -> RemoteDevice
        -> RemoteEndpoint
           -> I4 Protocol / Codec
           -> SimulatedTransport
           -> I4 Protocol / Codec
           -> EdgeHost
              -> I3 Device
                 -> I3 Endpoint
                    -> Mock Hardware
```

DeviceManager remains the one Core Component. RemoteDevice is a Device and RemoteEndpoint is an Endpoint; application code sees unchanged I3 contracts. EdgeHost serves sealed I3 devices. Transport owns virtual time. Protocol owns messages/framing, not device semantics.

I4 session states: DISCONNECTED -> CONNECTING -> NEGOTIATING -> CONNECTED -> DEGRADED -> DISCONNECTED. These are not Core LifecycleState values.

Core R1.0 is immutable. Verified Core primitives are `kritva::core::messaging::MessageHeader`, `Topic`, and `kritva::core::time::IClock`; there is no Core wire codec or transport contract. I4 supplies the codec, protocol and simulated clock implementation.

Edge is safety authority. Write processing: validate session/address/state -> validate command/limits -> reject replay/duplicate -> apply exactly once -> respond. Heartbeat timeout stops affected actuator endpoints through existing stop().
