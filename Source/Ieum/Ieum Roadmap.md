# Ieum Commercial Roadmap

`Ieum` is the commercial-grade binding platform that connects interaction outputs, control sources, and automation streams to parameter targets across the Dadeum ecosystem. In the current product family, its first concrete responsibility is to connect `Gyeol` widget outputs to `Teul` audio parameters. Its architectural goal, however, is broader: to become a durable control-binding platform that can accept multiple source systems, multiple target systems, multiple transform policies, and future external input providers such as `Naru` without redesigning the engine core.

---

## 1. Product Definition

### 1.1 Product Mission

Build a production-ready binding platform that:

- exposes inputs from interactive systems as reusable control sources,
- exposes parameters from runtime systems as reusable targets,
- resolves, validates, transforms, and applies bindings reliably,
- preserves state through save/load, preset, and recovery flows,
- communicates health and failure states clearly,
- remains extensible as new products and input providers are added.

### 1.2 Product Positioning

`Ieum` is not a single editor panel and not a one-off `Gyeol -> Teul` feature. It is the control-routing and binding layer of the Dadeum stack. Its long-term value is that any source-producing system and any target-consuming system can integrate once and then share a common binding workflow.

### 1.3 Non-Goals

The first commercial version should **not** try to:

- become a full scripting language,
- directly own source-specific UI logic that belongs in source products,
- directly own target-specific DSP semantics that belong in target products,
- hardcode binding rules for every known widget or parameter pair,
- bury failure handling inside ad hoc UI code.

---

## 2. Business and User Outcomes

### 2.1 Business Outcomes

- One binding platform replaces multiple product-specific connection systems.
- New source systems and new target systems can be added with lower integration cost.
- Persistence, recovery, diagnostics, and UX become consistent across products.
- Support and QA get one canonical status model for missing, degraded, and invalid bindings.

### 2.2 User Outcomes

- Users can map visual controls to audio behavior without code.
- Bindings remain understandable after document edits, preset changes, or product updates.
- Failed bindings explain themselves instead of silently disappearing.
- Advanced control workflows remain possible without turning the product into a hard-to-maintain patchwork.

### 2.3 Internal Outcomes

- The source side and target side can evolve independently.
- Core logic is provider-driven rather than hardcoded to one product pair.
- Engineering effort shifts from ad hoc fixes to reusable adapters and transforms.

---

## 3. Commercial Product Requirements

### 3.1 Functional Requirements

- Publish bindable sources from `Gyeol`.
- Publish bindable targets from `Teul`.
- Support provider-based expansion for future systems such as `Naru`.
- Support direct and grouped bindings.
- Support transform chains and multiple binding modes.
- Support document persistence, preset integration, and recovery.
- Support health states such as:
  - `ok`
  - `missing`
  - `degraded`
  - `invalid`
- Support editor inspection, filtering, search, and editing.

### 3.2 Non-Functional Requirements

- Stable identifier-based resolution.
- Deterministic runtime behavior.
- Clear validation semantics.
- Extensible schemas and registries.
- Compatibility-aware persistence.
- Sufficient diagnostics for support and QA.

### 3.3 Commercial Readiness Requirements

- Save/load resilience.
- Schema evolution strategy.
- Explicit migration rules.
- Reproducible diagnostics.
- Clear user-visible failure states.
- Safe fallback behavior when bindings cannot resolve.

---

## 4. Stakeholders and Operating Context

### 4.1 Stakeholders

- Product owner
- UI systems engineer
- audio systems engineer
- runtime/platform engineer
- interaction designer
- QA
- support / operations

### 4.2 Operating Context

- local editing session
- live interaction session
- preset browsing session
- autosave and crash recovery session
- multi-document project environment

### 4.3 Risk Context

- widget rename or deletion
- target parameter rename or replacement
- schema evolution across versions
- multiple sources competing for one target
- binding loops or ambiguous transform chains
- restore after incomplete product state

---

## 5. Architecture Principles

### 5.1 Provider-Driven Core

The core must not know specific widget classes or parameter classes. It should depend on:

- `SourceProvider`
- `TargetProvider`
- `TransformRegistry`
- `BindingResolver`
- `BindingRuntime`
- `BindingPersistence`
- `EditorAdapters`

### 5.2 Identifier and Schema First

All persistent bindings must reference stable identifiers and provider-declared schemas, not live object pointers.

### 5.3 Capability-Based Validation

Compatibility should be driven by capabilities and metadata, not by hardcoded source-target pairs.

### 5.4 Transform Pipelines over Special Cases

Use explicit transform chains rather than hidden source-specific or target-specific conversions.

### 5.5 First-Class Status Model

The same status model must drive:

- runtime resolution,
- editor badges,
- tooltips,
- persistence metadata,
- recovery behavior,
- diagnostics.

### 5.6 Separation of Concerns

Keep these concerns separate:

- source publication,
- target publication,
- binding storage,
- resolution and validation,
- transform execution,
- runtime application,
- editing UX,
- diagnostics and migration.

---

## 6. Core Domain Model

### 6.1 Source Descriptor

- `ProviderId`
- `EntityId`
- `OutputId`
- `OutputKind`
- `ValueType`
- `Capabilities[]`
- `Metadata`

### 6.2 Target Descriptor

- `ProviderId`
- `EntityId`
- `ParamId`
- `ParamType`
- `AcceptedModes[]`
- `RangeMetadata`
- `Capabilities[]`
- `Metadata`

### 6.3 Binding Spec

- `BindingId`
- `SourceRef`
- `TargetRef`
- `BindingMode`
- `TransformChain[]`
- `Enabled`
- `Priority`
- `GroupId`
- `LastResolvedStatus`
- `StatusDetails`
- `Version`

### 6.4 Binding Group

- `GroupId`
- `GroupType`
- `AggregationPolicy`
- `MemberBindings[]`

### 6.5 Runtime Binding State

- `ResolvedSourceHandle`
- `ResolvedTargetHandle`
- `ResolvedTransformPipeline`
- `RuntimeStatus`
- `LastAppliedValue`
- `LastError`

### 6.6 Editor State

- selected binding
- selected source
- selected target
- filter state
- search state
- visibility mode
- validation overlay state

---

## 7. Extensibility Contracts

### 7.1 Source Provider Contract

Must provide:

- published source entities,
- source descriptors,
- stable identifiers,
- capability declarations,
- runtime value observation hooks,
- provider version.

### 7.2 Target Provider Contract

Must provide:

- published target entities,
- target descriptors,
- stable identifiers,
- accepted binding modes,
- parameter metadata,
- write adapter hooks,
- provider version.

### 7.3 Transform Registry Contract

Must support:

- transform registration,
- transform versioning,
- transform capability requirements,
- deterministic execution order,
- serialization/deserialization hooks.

### 7.4 Editor Adapter Contract

Must support:

- source browsing,
- target browsing,
- schema-driven field rendering,
- state badge rendering,
- diagnostics surfaces.

---

## 8. Status, Recovery, and Compatibility Model

### 8.1 Status Definitions

- `ok`
  - source and target resolve successfully and are valid
- `missing`
  - source or target cannot be found
- `degraded`
  - binding can partially resolve, but not to its original shape or quality
- `invalid`
  - source and target resolve, but the binding contract is no longer valid

### 8.2 Recovery Rules

- Never silently discard unresolved bindings.
- Preserve unresolved references for later recovery.
- Persist status details so recovery is explainable.
- Prefer explicit degraded state over implicit fallback behavior.

### 8.3 Compatibility Rules

- Binding schema must be versioned.
- Provider schema changes must have migration rules.
- Transform schema changes must have migration or rejection policy.
- Unsupported bindings must fail visibly, not implicitly.

---

## 9. Security, Safety, and Reliability

### 9.1 Data Safety

- Bindings must be crash-safe under autosave and recovery.
- Corrupted binding documents must degrade predictably.
- Unknown provider identifiers must not crash the editor or runtime.

### 9.2 Runtime Safety

- Invalid bindings must not spam target writes.
- Broken providers must not poison unrelated bindings.
- Multi-source aggregation rules must be deterministic.

### 9.3 Reliability

- The same document should resolve the same way on repeated load, given the same provider state.
- Recovery after partial product startup should produce `missing` or `degraded`, not silent no-op behavior.

---

## 10. Observability and Operations

### 10.1 Diagnostics Requirements

Expose:

- provider presence
- provider version
- source availability
- target availability
- binding counts by status
- transform pipeline failures
- last migration result
- last recovery result

### 10.2 Logging Requirements

Use structured logs for:

- binding create/update/delete
- resolve failure
- degraded fallback
- invalid migration
- provider mismatch
- preset override application
- recovery restore result

### 10.3 Support Tooling

Add:

- binding diagnostics inspector
- source availability viewer
- target availability viewer
- migration report viewer
- unresolved binding export

---

## 11. UX Requirements

### 11.1 User-Facing UX

- Clear source and target browsing.
- Clear binding creation path.
- Clear distinction between valid, missing, degraded, and invalid.
- Search and filter that scale with many sources and targets.
- Binding editing that does not require knowing engine internals.

### 11.2 Advanced UX

- Grouped bindings.
- Macro-style views.
- Rich transform configuration.
- Source-to-many and many-to-one visibility.
- Preset-aware binding overview.

### 11.3 Developer UX

- Replayable binding states.
- Migration debugging.
- Provider schema inspection.
- Transform registry inspection.

---

## 12. Delivery Strategy

### 12.1 Release Tracks

- Internal architecture prototype
- Internal integration beta
- Closed workflow beta
- Production candidate
- Commercial release

### 12.2 Gate Policy

Every release track must define:

- supported providers
- supported binding modes
- migration guarantees
- diagnostics completeness
- known limitations
- rollback strategy

### 12.3 Backward Compatibility Policy

- Binding document schema must be versioned.
- Additive changes should be non-breaking where possible.
- Breaking schema changes require migration or explicit invalidation behavior.

---

## 13. Full Implementation Roadmap

### Phase 1. Provider contracts and schema model [DONE]
Goal: lock the generic interfaces before product-specific code spreads.

Work:
- define `SourceProvider`
- define `TargetProvider`
- define source schema structure
- define target schema structure
- define capability taxonomy
- define versioning rules

Deliverables:
- provider contracts
- schema contracts
- compatibility matrix draft

Exit criteria:
- the core can reason about sources and targets without hardcoded widget/parameter classes

### Phase 2. Binding model and persistence contract [DONE]
Goal: define the durable storage model before editor features depend on unstable fields.

Work:
- define binding spec
- define binding group model
- define transform chain storage
- define status model
- define document serialization format

Deliverables:
- binding schema
- persistence contract
- migration policy draft

Exit criteria:
- new providers can be added without redesigning binding storage

### Phase 3. Resolution and validation engine [IN PROGRESS]
Goal: resolve stored binding references into live runtime handles and compute health state.

Work:
- implement provider registry
- implement source resolve
- implement target resolve
- implement capability compatibility checks
- implement mode compatibility checks
- compute `ok`, `missing`, `degraded`, `invalid`

Deliverables:
- binding resolver
- validation engine
- status details model

Exit criteria:
- bindings can be validated without hardcoded source-target pairs

### Phase 4. Transform pipeline engine
Goal: make all value conversion and shaping extensible.

Work:
- implement transform registry
- implement base transforms
- implement transform validation
- implement deterministic pipeline execution
- define transform migration rules

Deliverables:
- transform runtime
- transform metadata system

Exit criteria:
- new transforms can be added without redesigning the binding model

### Phase 5. Runtime application engine
Goal: make resolved bindings affect live targets safely.

Work:
- connect source event ingestion
- implement target write adapters
- define multi-source aggregation policy
- define scheduling/throttling/coalescing behavior
- implement error containment around target writes

Deliverables:
- runtime binding engine
- target write layer

Exit criteria:
- generic providers can drive generic targets through one runtime path

### Phase 6. Editor foundation
Goal: make bindings visible and editable in the product.

Work:
- source browser
- target browser
- binding list
- schema-driven inspector
- search and filtering
- status badge rendering

Deliverables:
- editor v1
- inspector v1
- diagnostics surfaces v1

Exit criteria:
- users can create and inspect bindings without code changes

### Phase 7. Persistence, recovery, and preset integration [PARTIAL]
Goal: make binding state durable across real product lifecycles.

Work:
- document save/load
- autosave/recovery
- preset override policy
- rename/replace/delete recovery strategy
- migration executor

Deliverables:
- recovery-aware persistence
- migration report path

Exit criteria:
- bindings remain explainable after load, restore, rename, and replacement scenarios

### Phase 8. Advanced control features
Goal: move from baseline binding to production-grade control workflows.

Work:
- relative mode
- trigger and toggle mode
- macro binding
- grouped aggregation
- conditional binding
- conflict resolution policy
- scene-aware binding

Deliverables:
- advanced binding rule set
- macro/group behavior definitions

Exit criteria:
- both simple and complex control workflows are supported by the same engine

### Phase 9. External provider readiness
Goal: make the platform ready for additional providers such as `Naru`.

Work:
- add external-source provider path
- define gesture and derived-source schema support
- validate mixed-provider editing workflows
- ensure diagnostics remain uniform across providers

Deliverables:
- provider expansion path
- mixed-provider validation rules

Exit criteria:
- external sources enter the platform through the same provider model

### Phase 10. Commercial diagnostics and support tooling
Goal: make the platform operable in QA and support contexts.

Work:
- add binding diagnostics inspector
- add unresolved binding browser
- add migration report UI
- add provider status panel
- add exportable diagnostics snapshot

Deliverables:
- support tooling
- QA tooling

Exit criteria:
- production binding issues can be diagnosed without engineering guesswork

### Phase 11. Hardening and production validation
Goal: reach commercial readiness.

Work:
- migration regression tests
- provider mismatch tests
- recovery tests
- preset override tests
- large-document tests
- grouped binding stress tests
- performance profiling

Deliverables:
- validation matrix
- release checklist
- production readiness report

Exit criteria:
- known failure modes are documented
- release gates are satisfied
- rollback plan exists

---

## 14. Release Milestones

### I1. Generic Binding Foundation Ready
- Phase 1 complete [DONE]
- Phase 2 complete [DONE]
- Phase 3 baseline complete

### I2. Generic Runtime Binding Ready
- Phase 3 complete
- Phase 4 complete
- Phase 5 complete

### I3. Editable Binding Platform Ready
- Phase 6 complete
- Phase 7 complete
- Phase 8 baseline complete

### I4. External-Provider Ready Platform
- Phase 8 complete
- Phase 9 complete
- Phase 10 baseline complete

### I5. Production Release Ready
- Phase 10 complete
- Phase 11 complete

---

## 15. Success Metrics

Track at minimum:

- binding resolution success rate
- missing binding rate
- degraded binding rate
- invalid binding rate
- recovery restore success rate
- preset override success rate
- binding editor operation success rate
- binding-related crash-free session rate
- migration success rate

---

## 16. Final Definition of Done

`Ieum` is commercially ready when:

- the core is provider-driven rather than widget- or parameter-hardcoded,
- bindings can be resolved, validated, transformed, applied, saved, restored, and inspected through one coherent model,
- `Gyeol`, `Teul`, and future providers such as `Naru` fit through the same contracts,
- diagnostics and recovery are productized,
- release and migration gates are met,
- future source and target systems can be added without redesigning the engine core.
