# ADR-0014: In-memory evidence review

**Status: Accepted for Phase 5D application-layer implementation.**

## Context

Phase 5C presents candidate observations and source provenance in the optional
Qt PDF workbench. The domain library already defines `ReviewState`,
`ReviewDecision`, `validate_review_decision()`, and
`apply_review_decision()` with a constrained transition lifecycle. The UI needs
to collect explicit human review input without changing extraction output or
the production-library graph.

## Decision

- Add a Qt-free application `ReviewSession` containing copies of observations
  and matching `EvidenceLocation` metadata. Key all decisions by
  `ObservationId`.
- Use existing domain validation and `apply_review_decision()` exclusively.
  Apply to a copy first and replace session state only after success.
- Enforce evidence ownership and replacement observation component/source
  context at the application boundary.
- Keep sessions volatile and visibly disclose that decisions are lost when
  discarded. Do not persist or export review data.
- Keep Qt and Qt Svg in the optional PDF workbench application. Embed six
  original supplied icons through Qt Resource System; visible state text remains
  authoritative.
- Do not promote observations to `ComponentKnowledge`, approve mathematical
  models, or start simulations.

## Consequences

Review metadata is not authenticated review authority. A `human_engineer` actor
kind and reviewer ID are declarations made by the application user; they do not
prove identity, authorization, or non-repudiation. In-memory history cannot
substitute for a persistent audit system. Observation approval does not validate
a model or its applicability.

Qt Svg is an additional optional PDF-workbench runtime/build dependency. Core,
production libraries, Poppler parsing, source fingerprint semantics, and
numerical simulation remain unchanged.
