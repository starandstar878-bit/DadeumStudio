# Ieum Roadmap

`Ieum` is the binding platform that connects interaction outputs from `Gyeol` to parameter targets in `Teul`. The design goal is not to hardcode widget-to-parameter mappings, but to build a general binding engine that can accept new input systems, new target systems, and new transform logic without redesigning the core.

---

## Design Direction

- Treat `Ieum` as a generic binding engine, not as a `Gyeol -> Teul` shortcut layer.
- Connect project-specific systems through provider and adapter interfaces.
- Keep the core stable when widget types, target types, or external sources increase.
- Move project-specific logic out of the core into schemas, capabilities, registries, and adapters.
- Make runtime state, persistence state, and editor state share the same binding model.

## Primary Goals

- Expose `Gyeol` widget outputs as reusable binding sources.
- Expose `Teul` parameters as reusable binding targets.
- Support one-to-many, many-to-one, and grouped binding scenarios without special-case code.
- Preserve binding state across document save/load, preset changes, and recovery flows.
- Keep the engine ready for future providers such as `Naru`.

## System Roles

- `Source Provider`
  - Publishes bindable outputs.
  - Today: `Gyeol`.
  - Future: `Naru`, external devices, automation systems.
- `Target Provider`
  - Publishes bindable parameters.
  - Today: `Teul`.
  - Future: additional engines or UI systems.
- `Ieum Core`
  - Owns binding model, resolution, validation, transform pipelines, runtime application, and persistence.
- `Ieum Editor`
  - Owns creation, inspection, modification, and status display of bindings.

## Core Principles

### 1. Identifier-first references
- Never store raw object pointers in documents.
- Bindings reference sources and targets by stable provider-scoped identifiers.
- Providers resolve identifiers into live runtime handles.

### 2. Schema-driven exposure
- Do not hardcode per-widget or per-parameter rules into the core.
- Source schemas define:
  - value type
  - output channels
  - event kind
  - supported transform hints
  - capabilities
- Target schemas define:
  - accepted value type
  - accepted binding modes
  - range metadata
  - write policy
  - capabilities

### 3. Capability-based compatibility
- Compatibility is decided by capabilities, not by hardcoded named pairs.
- Example capabilities:
  - continuous
  - discrete
  - trigger
  - toggle
  - normalized
  - bipolar
  - stepped

### 4. Transform pipeline as registry
- Invert, remap, smoothing, curves, quantize, and relative logic should not be hardcoded into one giant apply function.
- The binding engine should execute a transform pipeline built from a transform registry.
- New transforms should be addable without changing the binding data model.

### 5. First-class status model
- `ok`, `missing`, `degraded`, and `invalid` are formal states, not just UI strings.
- The same state model should drive resolve output, persistence, UI badges, tooltips, and recovery logic.

---

## Extensible Data Model

### Source Descriptor
- `ProviderId`
- `EntityId`
- `OutputId`
- `OutputKind`
- `ValueType`
- `Capabilities[]`
- `Metadata`

### Target Descriptor
- `ProviderId`
- `EntityId`
- `ParamId`
- `ParamType`
- `AcceptedModes[]`
- `RangeMetadata`
- `Capabilities[]`
- `Metadata`

### Binding Spec
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

### Binding Group
- `GroupId`
- `GroupType`
- `AggregationPolicy`
- `MemberBindings[]`

### Runtime Binding State
- `ResolvedSourceHandle`
- `ResolvedTargetHandle`
- `ResolvedTransformPipeline`
- `RuntimeStatus`
- `LastAppliedValue`
- `LastError`

## Editor Principles

- Build inspectors from schemas instead of per-project hardcoded forms.
- Let providers supply source lists and target lists.
- Use a shared badge and status language across all binding UIs.
- Design search, filter, and grouping early so the system can scale when providers and assets grow.

---

## Implementation Order

### Phase 1. Provider and schema contracts
Goal: define the generic interfaces that all future source and target systems must implement.

Work:
- define `SourceProvider`
- define `TargetProvider`
- define source schema structure
- define target schema structure
- define capability model
- define resolve context

Done when:
- the core can reason about sources and targets without knowing `Gyeol` or `Teul` classes directly.

### Phase 2. Binding model and persistence contract
Goal: lock the document shape before building the editor.

Work:
- define binding spec
- define transform chain
- define binding group
- define status model
- define JSON/document format

Done when:
- new providers can be added without redesigning the saved binding format.

### Phase 3. Resolution and validation engine
Goal: resolve stored identifiers into live handles and calculate compatibility and state.

Work:
- implement provider registry
- implement source resolve
- implement target resolve
- implement capability compatibility
- implement mode compatibility
- calculate `ok`, `missing`, `degraded`, `invalid`

Done when:
- the engine can validate bindings without hardcoded widget/parameter names.

### Phase 4. Transform pipeline engine
Goal: move all binding value processing into an extensible transform stack.

Work:
- implement transform registry
- add base transforms
  - remap
  - invert
  - curve
  - smoothing
  - quantize
- define transform ordering
- validate transform compatibility

Done when:
- new transforms can be added without redesigning the binding core.

### Phase 5. Runtime apply engine
Goal: make resolved bindings update real targets reliably.

Work:
- connect source event ingestion
- implement target write adapters
- define multi-source aggregation policy
- define scheduling, throttling, and coalescing rules

Done when:
- `Gyeol` sources can drive `Teul` parameters reliably through the generic engine.

### Phase 6. Ieum editor v1
Goal: make bindings creatable and editable by users.

Work:
- source browser
- target browser
- binding list
- schema-driven binding inspector
- status badges and detail tooltips
- group view draft

Done when:
- users can create and inspect bindings without code changes.

### Phase 7. Save/load/recovery/preset integration
Goal: keep binding state stable across document lifecycle events.

Work:
- document save/load
- autosave/recovery
- preset override rules
- rename/replace/delete recovery strategy

Done when:
- bindings remain understandable and recoverable after structural changes.

### Phase 8. Advanced binding rules
Goal: raise the engine from basic control to production-grade control logic.

Work:
- relative mode
- trigger and toggle mode
- macro binding
- scene-aware binding
- conditional binding
- conflict resolution policy

Done when:
- live performance and precision editing scenarios are both supported.

### Phase 9. Naru provider integration
Goal: allow `Naru` to enter the engine as another source provider.

Work:
- add `NaruSourceProvider`
- define schema for gesture and continuous derived sources
- validate mixed `Gyeol` and `Naru` editing workflows

Done when:
- `Naru` inputs are first-class binding sources, not a special-case path.

### Phase 10. Validation and regression testing
Goal: make the platform resilient as providers and targets evolve.

Work:
- provider add/remove tests
- widget rename/delete tests
- node replace/param rename tests
- preset reload tests
- recovery restore tests
- missing/degraded/invalid scenario coverage

Done when:
- the engine survives provider growth and document changes without core redesign.

---

## Milestones

### I1. Generic binding foundation
- Phase 1 complete
- Phase 2 complete
- Phase 3 baseline complete

### I2. Generic runtime engine
- Phase 3 complete
- Phase 4 complete
- Phase 5 complete

### I3. Editable binding platform
- Phase 6 complete
- Phase 7 complete
- Phase 8 baseline complete

### I4. External-source ready platform
- Phase 8 complete
- Phase 9 complete
- Phase 10 complete

## Completion Criteria

- `Ieum` runs on provider and schema contracts rather than widget-specific hardcoding.
- New source systems and new target systems can be attached through adapters.
- Runtime, persistence, recovery, and UI all share the same binding state model.
- `Naru` or any future input system can be integrated without reworking the engine core.
