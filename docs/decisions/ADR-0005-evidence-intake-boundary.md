# ADR-0005: Evidence intake boundary (Phase 4A)

Date: 2026-11-04

Status: **IMPLEMENTED and MERGED in PR #6**, with the open policy choices below
remaining **PROPOSED**. The bounded in-memory contracts and
validation described in [PHASE_4_IMPLEMENTATION.md](../PHASE_4_IMPLEMENTATION.md)
are implemented without changing the existing domain modules. Related plan:
[Phase 4 plan](../PHASE_4_PLAN.md); test design:
[Phase 4 validation plan](../PHASE_4_VALIDATION_PLAN.md).

## Context

Phase 1/1.5 (PR #1), Phase 2 (PR #2) and Phase 3 (PR #3, merge `c142a36`) are merged
into `main`. The repository has verified in-memory contracts from
`SourceDocument` through `ComponentKnowledge`, `ModelArtifactDescriptor` and an
ideal-RC simulation, but **no code path creates `ParameterObservation` records from
external datasheet information**; every existing record is hand-built in
`tests/unit/component_fixtures.cpp`. Roadmap Stage 5 (assisted extraction) cannot be
evaluated until a deterministic, reviewable boundary exists for datasheet-derived
candidates. This ADR defines that boundary for manually curated or synthetic input
only (PDF/OCR/LLM remain out of scope).

Existing facts that constrain the design (verified by code inspection on
`c142a36`, see [Phase 4 plan §2](../PHASE_4_PLAN.md)):

- `ParameterObservation` already preserves `parameter_name`, `original_value_text`,
  `original_unit_symbol`, `parsed_numeric_value`, `qualifier`, `category`,
  `operating_conditions`, `evidence_refs`, `extraction_method`, `extraction_tool`,
  `uncertainty_note`, `review_state`, and `review_history`.
- `validate_review_decision` rejects `ActorKind::llm_assistant` and
  `ActorKind::automatic_parser` for **every** transition, and requires
  `ActorKind::human_engineer` for `approved`, `rejected`, and `superseded`.
- `validate_domain_bundle` resolves evidence-to-document and observation-to-evidence
  references but **does not** compare manufacturer/publisher or `part_number_suffix`
  with a document (`is_source_document_applicable` compares only
  `ComponentIdentity::part_number` with `SourceDocument::associated_part_numbers`),
  and does not bound `EvidenceLocation::pdf_page_index` (no page-count field exists).
  A scratch probe on `c142a36` confirmed these three behaviors.
- `ComponentDomainBundle` is a single-component aggregate (ADR-0003, ADR-0004).

## Decision (PROPOSED)

1. **A separate intake layer, `datasim_intake` (PROPOSED)**, in
   `namespace datasim::intake` under `src/datasim/intake/`, linking only
   `datasim_component`. It must not link `datasim` or `datasim_vertical`, must not
   modify `datasim_component` contracts, and must not call
   `promote_parameter_to_knowledge`, `make_normalized_parameter`, or
   `apply_review_decision`.
2. **Intake output is candidate-only.** The sole product is an in-memory
   `ComponentDomainBundle` whose observations all have
   `review_state == ReviewState::candidate`, empty `review_history`,
   `knowledge == std::nullopt`, and empty `models` / `model_validations`, plus
   intake diagnostics. Structural impossibility (no code path that sets another
   state) is the primary control; tests are secondary.
3. **Raw-versus-parsed separation.** Raw cell text is preserved verbatim in the
   existing `original_value_text` / `original_unit_symbol` / `parameter_name`
   fields. `parsed_numeric_value` is set only for a strict, unambiguous decimal or
   scientific literal **in the original unit**; it is never an SI conversion and
   never inferred from a range, tolerance, inequality, or footnoted value. Anything
   else stays `std::nullopt` with an intake diagnostic.
4. **No unit conversion, no qualifier merging, no category promotion** in intake.
   SI normalization stays in reviewed `NormalizedParameter` records
   (`conversion_rule`), per ADR-0004.
5. **Intake-level identity checks stricter than the domain bundle**
   (`PROPOSED`): exact manufacturer ↔ `SourceDocument::publisher` consistency
   (or an explicit, reviewed `publisher_differs_from_manufacturer` declaration),
   exact part number **and** suffix match against the document's applicable part
   numbers, and declared page-count bound for `pdf_page_index`. These checks live in
   intake and do not change Phase 2 behavior. Whether to harden the Phase 2 domain
   validator instead is an owner decision (D2 in the plan).
6. **Review remains the only promotion path.** Candidates reach
   `ComponentKnowledge` only through the existing lifecycle
   (`candidate → needs_review → approved`, human reviewer), existing
   `make_normalized_parameter` and `promote_parameter_to_knowledge`, existing
   `validate_domain_bundle`, and (for simulation) the existing Phase 3 gates.
   Intake never constructs a `ReviewDecision`.
7. **Provenance class is explicit and has no "verified" value.** Intake records
   one of `synthetic_fixture` or `manual_curation` (PROPOSED enum
   `IntakeOriginClass`). There is deliberately **no** value meaning
   "manufacturer-verified" or "authenticated"; any such claim is outside what
   in-memory metadata can establish.
8. **Fixtures first.** Phase 4B input is manually written C++ aggregate
   initialization of synthetic fixtures. No serialization format is selected.

## Metadata review versus authentication

Review in this design means a recorded decision, by an actor whose
self-declared `ReviewerIdentity::kind` is `human_engineer`, that candidate metadata
and the claimed relationship to a cited location are acceptable. It does **not**
authenticate the reviewer, the curator, the manufacturer, the document's origin, the
`content_sha256` value (which is an unauthenticated claim; the synthetic fixtures use
placeholder digests), or the PDF's bytes. FNV-1a content identities in Phase 3 are
non-cryptographic fingerprints. A reviewer who is also the curator is not prevented by
the domain contract (only `extraction_tool == reviewer.id` is); separation of curator
and approver is an open decision (D5).

## Alternatives considered

- **A. Extend `ParameterObservation`/`EvidenceLocation` with row, column, symbol,
  raw-qualifier, and ambiguity fields now.** Rejected for 4A: changes verified Phase 2
  contracts and their 270 checks before the need is demonstrated by fixtures. Kept as
  decision D3 (default: sidecar fields in intake records).
- **B. Put intake inside `datasim_component`.** Rejected: weakens the single-component,
  dependency-free boundary of ADR-0003 and mixes parsing-adjacent logic with contracts.
- **C. Fold intake into `datasim_vertical`.** Rejected: couples evidence handling to
  simulation (ADR-0004 keeps those separate).
- **D. Intake that auto-submits `needs_review` decisions.** Rejected for 4B: even this
  needs a non-LLM, non-parser actor (`automated_check` or a human); automatic state
  changes blur the evidence/review boundary. A later explicit review-queue step is
  proposed instead.
- **E. Select PDF, JSON, or database technology now.** Rejected: unrelated to the
  boundary; each needs its own ADR and licensing/security review.
- **F. Harden `is_source_document_applicable` in Phase 2 code.** Deferred to owner:
  improves all consumers but modifies a verified module, so requires explicit
  authorization (D2).

## Consequences

- A reviewable seam exists before any extractor is built; future extractors (table
  parser, OCR, LLM) can only emit the same candidate-only intake structures.
- Intake adds checks that reject real-world mismatches the domain bundle currently
  accepts, at the cost of a second validation layer to keep consistent.
- Sidecar fields (default D3) are not yet visible in `ComponentDomainBundle`; row and
  column locators are therefore carried as verbatim text in
  `EvidenceLocation::cell_or_region` until D3 is decided.
- Parsed values remain in original units, so the review/normalization step stays
  mandatory and visible.

## Phase 4B implementation notes

The implementation adds `datasim_intake` under `src/datasim/intake/`, linking
only `datasim_component`. It uses one manifest source document, explicit page
index provenance, declared page-count bounds, exact publisher/part checks, and
candidate-only bundle construction. Ambiguous raw numeric text is a warning when
no parsed value is asserted and an error when a parsed value is asserted. These
are implementation details of the feature branch, not claims that the owner has
accepted every open decision above.

The bounded 4B implementation and synthetic P-07 test are merged. Hosted run
[41995104973](https://github.com/quocthaile/datasim/actions/runs/41995104973)
executed the real ngspice path without a skip. Remaining policy choices in this
ADR are not implied to have been approved by that verification.

Metadata review is an owner decision: the current human review records metadata
acceptance for a candidate, but does not authenticate a source, hash, reviewer
identity, or manufacturer relationship.

## Limitations

Licensing: real vendor datasheets and model files must not be committed; fixtures are
synthetic and the repository has no adopted license yet. Security: document text is
data, never instructions; no hash authenticity or signature is established.
Reproducibility: intake output is a deterministic function of its input structures;
nothing is claimed about real-datasheet extraction accuracy.

Related: [ADR-0001](ADR-0001-simulation-backend-boundary.md),
[ADR-0003](ADR-0003-component-domain-model.md),
[ADR-0004](ADR-0004-phase-3-vertical-slice.md),
[Phase 4 plan](../PHASE_4_PLAN.md),
[Phase 4 validation plan](../PHASE_4_VALIDATION_PLAN.md).
