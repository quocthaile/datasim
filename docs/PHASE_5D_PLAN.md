# Phase 5D plan: In-memory engineering evidence review

## Objective

Extend the Phase 5C `EvidenceWorkbench` with an application-owned, volatile
review session that uses the existing `datasim::component::ReviewDecision`,
`ReviewState`, `validate_review_decision()`, and `apply_review_decision()`
contracts. Add the six supplied project SVG resources. Preserve candidate
extraction and the production library graph.

## Scope and boundaries

- Copy candidate `ParameterObservation` values and the matching
  `EvidenceLocation` metadata into a Qt-free C++20 `ReviewSession`.
- Index state by `ObservationId`; preserve the original `ExtractionResult` and
  `IntakeManifest`.
- Require decision evidence to belong to the selected observation and targets
  to share component and source-evidence context.
- Render legal transitions, reviewer ID, rationale, evidence reference,
  timestamp, explicit replacement ID for supersession, and history in the UI.
- Keep Qt and Qt Svg in the optional desktop application. Keep review state
  volatile and separate from candidate extraction, `ComponentKnowledge`, model
  validation, and simulation.

No database, serialization/export, authentication, collaboration, automatic
approval, model creation, parser changes, or simulation routing is authorized.

## Exit evidence

Clean core/sanitizer and desktop OFF/ON builds, all CTest results and skips,
hosted branch CI logs, and manual GUI lifecycle evidence are recorded in
[validation](PHASE_5D_VALIDATION.md). PR creation is review-only; automatic merge
is prohibited.
