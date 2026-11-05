# Datasim agent guidance

Read [README](README.md), [project guidance](docs/AGENTS.md), and the relevant
architecture/decision documents before changes. Phase 1 ([implementation](docs/PHASE_1_IMPLEMENTATION.md))
and Phase 1.5 ([reproducibility](docs/PHASE_1_REPRODUCIBILITY.md),
[architecture review](docs/PHASE_1_ARCHITECTURE_REVIEW.md)) are completed, merged into
`main` via PR #1 (`86b924b`), and VERIFIED on Windows x64 (MSYS2 UCRT GCC / ngspice-47)
within their ideal-RC scope. Preserve all Phase 1/1.5 behavior, the 1% step-amplitude
tolerance, and historical validation evidence, and distinguish hosted CI execution
from a local clean build.

Phase 2 ([plan](docs/PHASE_2_PLAN.md), [implementation](docs/PHASE_2_IMPLEMENTATION.md),
[validation](docs/PHASE_2_VALIDATION.md), [ADR-0003](docs/decisions/ADR-0003-component-domain-model.md))
is completed and merged into `main` via PR #2 (`b2f73d5`).

Phase 3 ([plan](docs/PHASE_3_PLAN.md),
[implementation](docs/PHASE_3_IMPLEMENTATION.md),
[validation](docs/PHASE_3_VALIDATION.md),
[ADR-0004](docs/decisions/ADR-0004-phase-3-vertical-slice.md)) is implemented and
merged into `main` at `6516a629c75662b83f48704bfd7978bcc299ff78` (verified in hosted
Windows CI run `41284202716`), with architecture and validation hardening on
`fix/phase3-validation-hardening`: deterministic component-to-simulation vertical
slice (`datasim_vertical`) connecting approved `ComponentKnowledge` and selected
`ModelArtifactDescriptor` instances to the existing ideal-RC simulation backend with
full provenance. Preserve user changes, do not commit directly to `main`, do not merge
pull requests, and do not implement PDF/OCR/datasheet ingestion, persistence, LLM
integration, GUI, semiconductor models, or a custom solver.

Phase 4B is implemented on `feat/phase-4b-evidence-intake`, with PR #4 merged
into that feature branch and hosted CI run `41962956370` successful for its
existing checks. The P-07 release-gate result still requires a non-skipped
ngspice run; Phase 4B is not merged into `main` or owner-approved.

Build and run CTest as documented. Integration skips are not passes. No LLM is a
numerical authority; manual RC parameters and synthetic fixtures are not
datasheet-extracted knowledge.
