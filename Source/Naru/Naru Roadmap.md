# Naru Commercial Roadmap

`Naru` is the commercial-grade real-time pen and gesture input platform for the Dadeum ecosystem. Its job is to ingest Apple Pencil input from iPad, normalize it into a stable cross-product input model, derive higher-level control and gesture signals, and distribute those signals to `Gyeol`, `Teul`, and `Ieum` through explicit adapters. The product goal is not merely to "stream stylus data", but to establish a durable input platform that can scale across devices, products, sessions, networks, and future feature sets without core redesign.

---

## 1. Product Definition

### 1.1 Product Mission

Build a production-ready input platform that:

- accepts raw pen input from iPad with low latency,
- converts that input into a product-neutral event and signal model,
- exposes continuous and discrete derived control sources,
- routes those sources into multiple Dadeum products safely,
- survives disconnects, version drift, and real-world user sessions,
- remains extensible for future device families beyond Apple Pencil.

### 1.2 Product Positioning

`Naru` is a platform layer, not a feature widget. It sits below product-specific UX and above device-specific capture. Its commercial value comes from:

- reducing duplicated input logic across products,
- making pen workflows consistent,
- enabling future hardware expansion,
- making gesture-derived automation a reusable product capability.

### 1.3 Non-Goals

The first commercial version should **not** aim to:

- become a general-purpose remote desktop input system,
- own application-specific editing logic,
- hardcode all behavior into one transport or one device,
- couple gesture recognition directly to `Gyeol`, `Teul`, or `Ieum`.

---

## 2. Business and User Outcomes

### 2.1 Business Outcomes

- One input platform supports multiple products instead of three separate input stacks.
- Product teams can add new pen behaviors without rewriting transport or normalization code.
- New device families can be evaluated with lower integration cost.
- Commercial support burden drops because diagnostics, session state, and recovery become centralized.

### 2.2 User Outcomes

- Users can draw, gesture, and modulate reliably with low perceived latency.
- Connection failures are visible, explainable, and recoverable.
- The same pen feels consistent across `Gyeol`, `Teul`, and `Ieum`.
- Derived sources such as pressure, tilt, speed, or gestures behave predictably.

### 2.3 Internal Outcomes

- Engineering teams get one canonical input schema.
- QA gets one shared validation matrix.
- Support gets one diagnostics model.
- Product design gets one language for pen-derived interactions.

---

## 3. Commercial Product Requirements

### 3.1 Functional Requirements

- Receive Apple Pencil samples from iPad.
- Track sessions, reconnection, timeouts, and heartbeats.
- Normalize position, pressure, tilt, azimuth, and phase.
- Derive reusable control streams such as:
  - normalized X/Y,
  - pressure,
  - tilt X/Y,
  - velocity,
  - acceleration,
  - stroke progress,
  - stroke length,
  - direction,
  - gesture triggers.
- Route normalized and derived signals to:
  - `Gyeol`,
  - `Teul`,
  - `Ieum`.
- Expose diagnostics, calibration, and health state.
- Persist device profiles and calibration settings.

### 3.2 Non-Functional Requirements

- Low end-to-end latency under normal network conditions.
- Graceful behavior under jitter, packet loss, and reconnect.
- Clear state model for degraded conditions.
- Stable versioning for transport and payload schemas.
- Observability sufficient for support and QA.
- Extensibility without rewriting the core pipeline.

### 3.3 Commercial Readiness Requirements

- Structured error handling.
- Product-safe diagnostics and logging.
- Crash-safe recovery where possible.
- Compatibility strategy for client/server version mismatch.
- Upgrade path for schema evolution.
- Support tooling and reproducibility.

---

## 4. Stakeholders and Operating Context

### 4.1 Stakeholders

- Product owner
- Input systems engineer
- Client engineer
- Server engineer
- UI/interaction engineer
- Audio/control systems engineer
- QA
- Support / operations

### 4.2 Operating Environments

- iPad client over local network
- Desktop host running Dadeum products
- Development lab environment
- User home studio environment
- Live performance environment

### 4.3 Risk Context

- Unstable Wi-Fi
- Session interruption mid-stroke
- Input bursts larger than expected
- Consumer-specific overload
- Version mismatch across products
- Device calibration drift

---

## 5. Architecture Principles

### 5.1 Adapter-Driven Core

The core must not be Apple Pencil-specific or product-specific. It should depend on:

- `DeviceAdapter`
- `TransportProvider`
- `NormalizationPipeline`
- `DerivedSourcePlugin`
- `GestureRecognizerPlugin`
- `ConsumerAdapter`

### 5.2 Explicit Lifecycle Boundaries

Separate the system into:

- device capture,
- transport,
- session management,
- normalization,
- derived source generation,
- routing,
- consumer integration,
- diagnostics and persistence.

### 5.3 Schema First

Define payloads, state models, and contracts before UI polish. Commercial reliability depends more on stable contracts than on early feature demos.

### 5.4 State Is a Product Surface

States such as:

- `connected`
- `reconnecting`
- `missing`
- `degraded`
- `timed-out`
- `version-mismatch`
- `invalid-session`

must be formal model values, not ad hoc strings.

### 5.5 Backward Compatibility Strategy

Version the transport protocol and normalized sample schema so older clients and newer hosts fail safely, not silently.

---

## 6. Core Domain Model

### 6.1 Device Sample

- `DeviceId`
- `DeviceFamily`
- `SessionId`
- `SampleId`
- `Timestamp`
- `Position`
- `Pressure`
- `Tilt`
- `Azimuth`
- `Phase`
- `DeviceMetadata`

### 6.2 Normalized Sample

- normalized position
- normalized pressure
- normalized tilt vector
- velocity
- acceleration
- stroke-relative time
- stroke-relative progress
- coordinate-space id

### 6.3 Derived Source Descriptor

- `SourceId`
- `SourceKind`
- `ValueType`
- `Capabilities[]`
- `Metadata`
- `ProviderScope`

### 6.4 Gesture Event

- `GestureId`
- `GestureKind`
- `Confidence`
- `StartTime`
- `EndTime`
- `GesturePayload`

### 6.5 Session State

- `TransportState`
- `RecoveryState`
- `LatencyEstimate`
- `PacketLossEstimate`
- `LastHeartbeatTime`
- `LastError`
- `ClientVersion`
- `ProtocolVersion`

### 6.6 Device Profile

- `ProfileId`
- `DeviceFamily`
- `PressureCurve`
- `TiltCalibration`
- `CoordinateCalibration`
- `NoiseThresholds`
- `SmoothingProfile`
- `Version`

---

## 7. Extensibility Contracts

### 7.1 Device Adapter Contract

Must provide:

- capture start/stop,
- session metadata,
- raw sample emission,
- device capability declaration,
- adapter version.

### 7.2 Transport Contract

Must provide:

- connection lifecycle,
- ordered or sequence-aware sample delivery,
- heartbeat,
- reconnect hooks,
- protocol version metadata.

### 7.3 Consumer Adapter Contract

Must provide:

- supported source subscriptions,
- delivery contract,
- consumer health reporting,
- backpressure or rejection feedback.

### 7.4 Plugin Contracts

Must support:

- registration,
- versioning,
- deterministic ordering,
- capability declaration,
- failure isolation.

---

## 8. Security, Privacy, and Safety

### 8.1 Security

- Define trust boundary between iPad client and host.
- Require session handshake with protocol version validation.
- Prevent malformed packet streams from crashing the host.
- Define acceptable authentication model for local-network commercial use.

### 8.2 Privacy

- Avoid collecting more metadata than needed for input routing.
- Make logs configurable so raw input history is not persisted accidentally.
- Separate diagnostics from user content where possible.

### 8.3 Runtime Safety

- Invalid or malformed streams must degrade gracefully.
- Consumer overload must not corrupt other consumers.
- Transport failure must not freeze product UIs.

---

## 9. Observability and Operations

### 9.1 Diagnostics Requirements

Expose:

- connection state,
- protocol version,
- session id,
- latency estimate,
- packet loss estimate,
- sample rate estimate,
- recovery attempts,
- last error.

### 9.2 Logging Requirements

Use structured logs for:

- session start/end,
- reconnect,
- version mismatch,
- packet decode failure,
- gesture recognition failure,
- consumer delivery rejection.

### 9.3 Support Tooling

Add:

- live session inspector,
- input preview,
- normalized sample monitor,
- gesture debug view,
- exportable diagnostics snapshot.

---

## 10. UX Requirements

### 10.1 User-Facing UX

- Clear connected/disconnected state.
- Clear degraded/recovering state.
- Calibration workflow that does not require engineering knowledge.
- Derived-source preview that users can understand.

### 10.2 Developer UX

- Easy local session bring-up.
- Replayable sample streams.
- Simulated transport issues.
- Consumer-side debug visibility.

### 10.3 QA UX

- Deterministic test scenarios.
- Recorded sample playback.
- Protocol compatibility test harness.

---

## 11. Delivery Strategy

### 11.1 Release Tracks

- Internal prototype
- Internal integration beta
- Closed product beta
- Production candidate
- Commercial release

### 11.2 Gate Policy

Each release track must define:

- supported devices,
- protocol version,
- diagnostics completeness,
- recovery expectations,
- known limitations,
- rollback plan.

### 11.3 Backward Compatibility Policy

- Minor protocol changes should be additive.
- Breaking changes require explicit version bump and compatibility behavior.
- Consumers must fail clearly on unsupported protocol versions.

---

## 12. Full Implementation Roadmap

### Phase 1. Platform contracts and domain model
Goal: lock the platform boundaries before implementation details spread.

Work:
- define core domain model
- define adapter interfaces
- define plugin boundaries
- define session state model
- define versioning policy

Deliverables:
- architecture spec
- domain model spec
- protocol compatibility rules

Exit criteria:
- every major subsystem has a stable contract
- Apple Pencil specifics are isolated to adapters

### Phase 2. Transport and session foundation
Goal: build a resilient live connection layer.

Work:
- implement transport server
- define packet framing
- implement session start/end
- implement heartbeat
- implement reconnect policy
- implement timeout policy

Deliverables:
- transport layer implementation
- session manager
- health state emitter

Exit criteria:
- connections can be established, lost, and restored predictably

### Phase 3. Apple Pencil device adapter
Goal: introduce the first production device path.

Work:
- implement iPad sample capture
- convert Apple Pencil data into generic device samples
- declare adapter capabilities
- emit raw sample stream into the platform

Deliverables:
- Apple Pencil adapter
- sample capture validation

Exit criteria:
- raw Apple Pencil data flows through the generic path

### Phase 4. Normalization pipeline
Goal: produce consumer-neutral samples.

Work:
- normalize coordinates
- normalize pressure and tilt
- compute velocity and acceleration
- segment strokes
- add calibration hook points
- define smoothing profiles

Deliverables:
- normalized sample pipeline
- calibration integration points

Exit criteria:
- all consumers can use normalized samples instead of raw device values

### Phase 5. Derived source and gesture engine
Goal: convert sample streams into reusable control signals.

Work:
- add derived source plugins
- add gesture recognizer plugins
- implement base derived sources
- implement base gestures
- define gesture confidence model

Deliverables:
- derived source engine
- gesture engine

Exit criteria:
- continuous and discrete control signals are both available

### Phase 6. Consumer adapter integration
Goal: connect the platform to actual products.

Work:
- implement `GyeolConsumerAdapter`
- implement `TeulConsumerAdapter`
- implement `IeumConsumerAdapter`
- define subscription requirements per consumer

Deliverables:
- three initial consumer adapters
- consumer capability matrix

Exit criteria:
- all three products can consume the same platform through adapters

### Phase 7. Routing and subscription model
Goal: distribute inputs safely and predictably.

Work:
- define subscription model
- implement routing table or routing graph
- define consumer access policy
- define shared and exclusive routing rules
- implement backpressure or rejection behavior

Deliverables:
- routing engine
- subscription registry

Exit criteria:
- one session can be routed to multiple consumers under explicit policy

### Phase 8. Device profiles, calibration, and recovery
Goal: make the system robust across real-world sessions.

Work:
- implement device profiles
- implement calibration profiles
- implement reconnect restore
- implement degraded-state handling
- add version mismatch handling

Deliverables:
- persistent profile model
- recovery strategy
- diagnostics states

Exit criteria:
- device and session health are observable and recoverable

### Phase 9. Commercial diagnostics and support tooling
Goal: make the platform operable in support and QA contexts.

Work:
- add live session inspector
- add normalized sample preview
- add gesture debug view
- add latency and packet loss visibility
- add diagnostics snapshot export

Deliverables:
- support tooling
- QA tooling

Exit criteria:
- issues can be reproduced and diagnosed without engineering guesswork

### Phase 10. Product UX and calibration UX
Goal: make the platform usable by non-engineers.

Work:
- add user-facing connection states
- add calibration UI
- add derived-source preview
- add session/device inspector UX
- define failure and recovery messaging

Deliverables:
- user-facing Naru surfaces
- calibration workflow

Exit criteria:
- users can connect, verify, calibrate, and recover without hidden steps

### Phase 11. Hardening and production validation
Goal: reach commercial readiness.

Work:
- packet loss testing
- jitter testing
- reconnect testing
- long-session soak testing
- rapid gesture switching tests
- multi-consumer routing tests
- compatibility tests across protocol versions

Deliverables:
- validation matrix
- release checklist
- production readiness report

Exit criteria:
- known failure modes are documented
- release gates are met
- rollback plan exists

---

## 13. Release Milestones

### N1. Core Platform Ready
- Phase 1 complete
- Phase 2 complete
- Phase 3 baseline complete

### N2. Reusable Input Pipeline Ready
- Phase 4 complete
- Phase 5 complete

### N3. Product Integration Ready
- Phase 6 complete
- Phase 7 complete

### N4. Commercial Stability Ready
- Phase 8 complete
- Phase 9 complete
- Phase 10 baseline complete

### N5. Production Release Ready
- Phase 10 complete
- Phase 11 complete

---

## 14. Success Metrics

Track at minimum:

- session success rate
- reconnect success rate
- median and p95 latency
- packet loss rate
- gesture recognition reliability
- consumer delivery failure rate
- crash-free session rate
- calibration completion rate

---

## 15. Final Definition of Done

`Naru` is commercially ready when:

- the core is adapter-driven rather than Apple Pencil-hardcoded,
- normalized and derived inputs are stable and reusable,
- `Gyeol`, `Teul`, and `Ieum` consume the platform through adapters,
- diagnostics, recovery, and calibration are productized,
- release gates and validation targets are met,
- future device families can be added without restructuring the core.
