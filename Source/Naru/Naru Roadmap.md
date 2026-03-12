# Naru Roadmap

`Naru` is the input platform that receives Apple Pencil data from iPad and turns it into normalized, reusable control streams for `Gyeol`, `Teul`, and `Ieum`. The architectural goal is not to build an Apple Pencil-only bridge, but to create a general real-time input platform that can accept new device adapters and new consumer adapters over time.

---

## Design Direction

- Treat `Naru` as a reusable input pipeline, not as a one-off transport helper.
- Separate device capture, transport, normalization, derived source generation, routing, and consumer integration.
- Keep the core agnostic to Apple Pencil specifics wherever possible.
- Make session state, diagnostics, and recovery part of the platform design from the beginning.
- Design for multiple future input devices, not just one tablet client.

## Primary Goals

- Receive stylus input from iPad with low latency.
- Convert raw device samples into a normalized, application-independent input model.
- Publish continuous and discrete derived sources that multiple products can consume.
- Route the same input stream to `Gyeol`, `Teul`, and `Ieum` through adapters instead of app-specific branches.
- Support device growth, transport changes, and recovery without redesigning the core.

## System Roles

- `Input Device Adapter`
  - Captures raw samples from one device family.
  - Today: Apple Pencil on iPad.
  - Future: other stylus devices, touch sources, motion sources.
- `Transport Layer`
  - Moves sample packets and maintains sessions.
- `Normalization Layer`
  - Converts raw device coordinates and values into a shared sample model.
- `Derived Source Engine`
  - Produces pressure, tilt, velocity, stroke progress, and gesture outputs.
- `Routing Layer`
  - Dispatches normalized and derived inputs to consumers.
- `Consumer Adapter`
  - Converts the shared input model into the shape required by a product such as `Gyeol`, `Teul`, or `Ieum`.

## Core Principles

### 1. Device adapters isolate hardware specifics
- Apple Pencil details belong inside the Apple Pencil adapter.
- The core should only work with generic device samples.

### 2. Transport is not interpretation
- Transport manages packets, heartbeat, reconnect, and ordering.
- Meaning extraction such as gesture recognition or smoothing belongs above transport.

### 3. Normalized samples are the contract
- Consumers should not depend on raw device values.
- All downstream systems read normalized samples or derived sources.

### 4. Routing is adapter-based
- The core should not hardcode `if target is Gyeol` style branches.
- Consumer adapter registration should decide how each product subscribes and receives data.

### 5. Session and health state are first-class
- `connected`, `reconnecting`, `missing`, `degraded`, and timeout states must be formal model values.
- Diagnostics, UI, and recovery logic should share the same state model.

---

## Extensible Data Model

### Device Sample
- `DeviceId`
- `SessionId`
- `SampleId`
- `Timestamp`
- `Position`
- `Pressure`
- `Tilt`
- `Azimuth`
- `Phase`
- `DeviceMetadata`

### Normalized Sample
- normalized position
- normalized pressure
- tilt vector
- velocity
- acceleration
- stroke-relative time
- coordinate space id

### Derived Source Descriptor
- `SourceId`
- `SourceKind`
- `ValueType`
- `Capabilities[]`
- `Metadata`

### Gesture Event
- `GestureId`
- `GestureKind`
- `Confidence`
- `StartTime`
- `EndTime`
- `GesturePayload`

### Session State
- `TransportState`
- `LatencyEstimate`
- `PacketLossEstimate`
- `RecoveryState`
- `LastError`

## Extension Points

- `DeviceAdapterRegistry`
- `TransportProvider`
- `NormalizationProfile`
- `DerivedSourcePlugin`
- `GestureRecognizerPlugin`
- `ConsumerAdapterRegistry`

If these are stable, Apple Pencil becomes only the first adapter, not the permanent shape of the platform.

---

## Implementation Order

### Phase 1. Core contracts and plugin boundaries
Goal: define the shared abstractions that every device and every consumer must follow.

Work:
- define device sample interface
- define normalized sample structure
- define device adapter interface
- define consumer adapter interface
- define session state model

Done when:
- Apple Pencil-specific logic is no longer required inside core types.

### Phase 2. Transport protocol and session contract
Goal: define a stable protocol for real-time sample delivery.

Work:
- define sample packet structure
- define session start/end/heartbeat
- define timestamp policy
- define reconnect policy
- add versioning strategy

Done when:
- transport can evolve without redesigning downstream consumers.

### Phase 3. Apple Pencil device adapter
Goal: implement the first real device adapter.

Work:
- collect raw Apple Pencil samples on iPad
- convert them into generic device samples
- extract pressure, tilt, azimuth, and phase

Done when:
- Apple Pencil input enters the system as generic samples.

### Phase 4. Server bridge and session manager
Goal: receive device streams on the server and manage connection state.

Work:
- implement transport server
- implement session manager
- implement timeout, reconnect, and heartbeat handling
- expose diagnostics state

Done when:
- connection lifecycle is tracked as structured session state.

### Phase 5. Normalization pipeline
Goal: convert raw device samples into shared consumer-ready samples.

Work:
- normalize coordinates
- smooth pressure
- normalize tilt
- compute velocity and acceleration
- segment strokes
- add calibration profile integration points

Done when:
- downstream systems can consume the normalized model only.

### Phase 6. Derived source and gesture engine
Goal: turn sample streams into reusable control sources.

Work:
- generate pressure, speed, tilt, stroke progress, and direction sources
- add gesture recognizer plugin system
- implement tap, hold, flick, and scrub recognizers

Done when:
- the platform can publish both continuous and discrete derived outputs.

### Phase 7. Consumer adapters
Goal: let multiple products consume the same platform through adapters.

Work:
- implement `GyeolConsumerAdapter`
- implement `TeulConsumerAdapter`
- implement `IeumConsumerAdapter`
- define consumer-specific mapping policies

Done when:
- the routing core no longer needs product-specific branches.

### Phase 8. Routing, subscription, and access policy
Goal: distribute normalized input safely to one or more consumers.

Work:
- define subscription model
- define source selection rules
- implement routing graph or dispatch table
- define exclusivity and shared-access policy

Done when:
- one input stream can be shared or isolated according to policy rather than hardcoded rules.

### Phase 9. Device profiles, recovery, and health state
Goal: make the platform resilient to disconnects and hardware variance.

Work:
- add device profile model
- add calibration profile model
- implement reconnect restore
- add `missing`, `degraded`, and invalid-session states
- define diagnostics and tooltip rules

Done when:
- device and network problems produce predictable states and recovery behavior.

### Phase 10. UX and debug tooling
Goal: make the platform inspectable and tunable by humans.

Work:
- input source preview UI
- latency and packet loss indicators
- calibration UI
- gesture debug view
- device/session inspector

Done when:
- developers and users can see and tune the input system without guessing.

### Phase 11. Validation and regression testing
Goal: ensure the platform survives growth in devices and consumers.

Work:
- network jitter tests
- packet drop tests
- reconnect tests
- long stroke tests
- rapid gesture switching tests
- multi-consumer routing tests
- new adapter integration tests

Done when:
- new devices and new consumers can be added without redesigning the core.

---

## Milestones

### N1. Platform foundation
- Phase 1 complete
- Phase 2 complete
- Phase 3 baseline complete

### N2. Input pipeline established
- Phase 3 complete
- Phase 4 complete
- Phase 5 complete
- Phase 6 baseline complete

### N3. Multi-consumer integration
- Phase 6 complete
- Phase 7 complete
- Phase 8 complete

### N4. Stable and extensible platform
- Phase 9 complete
- Phase 10 complete
- Phase 11 complete

## Completion Criteria

- `Naru` remains adapter-driven rather than Apple Pencil hardcoded.
- Normalization, derived sources, gesture logic, routing, and state handling stay separated.
- `Gyeol`, `Teul`, and `Ieum` consume the same platform through consumer adapters.
- Additional devices can be introduced without redesigning the core pipeline.
