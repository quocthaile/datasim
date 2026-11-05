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

Phase 4A and Phase 4B are merged into `main` by normal-merge PRs #5 and #6;
Phase 4B baseline is `7d9af1c88929fe3c29d55e1c2438e8d2041acb2a`, verified by
hosted Windows CI run `41995104973` with a non-skipped P-07 ngspice path.
Phase 4C ([plan](docs/PHASE_4C_PLAN.md), [implementation](docs/PHASE_4C_IMPLEMENTATION.md),
[validation](docs/PHASE_4C_VALIDATION.md), [ADR-0006](docs/decisions/ADR-0006-deterministic-pdf-table-extraction.md))
is implemented, merged into `main` via PR #7 (`1704f726d901f3e1d00f9cdc918e80d352b443ac`),
and verified on synthetic fixtures by hosted Windows CI runs `42000652152` (PR #7 head)
and `42017272043` (post-merge `main`). Phase 4D ([plan](docs/PHASE_4D_PLAN.md),
[implementation](docs/PHASE_4D_IMPLEMENTATION.md), [validation](docs/PHASE_4D_VALIDATION.md))
connects deterministic PDF extraction (`datasim_pdf`) to `datasim_intake`,
`datasim_component`, `datasim_vertical`, and real `ngspice`, merged into `main`
via PR #8 (`69aba4df7ff752355a40e394de5d05ac988df34b`) and verified by hosted
Windows CI run `42025406013`. Phase 4E ([plan](docs/PHASE_4E_PLAN.md),
[corpus protocol](docs/PHASE_4E_CORPUS_PROTOCOL.md),
[validation](docs/PHASE_4E_VALIDATION.md)) implements the offline evaluation
corpus and benchmark, merged into `main` via PR #9
(`b45c32224e0316dd9f93b6e9bfc3ab89cd3ceaf4`) and verified by hosted Windows CI
run `42037281172`. Phase 4F ([plan](docs/PHASE_4F_PLAN.md),
[implementation](docs/PHASE_4F_IMPLEMENTATION.md),
[validation](docs/PHASE_4F_VALIDATION.md),
[ADR-0007](docs/decisions/ADR-0007-explicit-header-binding.md)) implements
explicit, source-verified column-header unit and condition bindings inside
`datasim_pdf`, merged into `main` via PR #10
(`9f3c115d13a40204d3d7f83382a50c107b14b803`) and verified by hosted Windows CI
run `42044651168`. Phase 4G ([plan](docs/PHASE_4G_PLAN.md),
[implementation](docs/PHASE_4G_IMPLEMENTATION.md),
[validation](docs/PHASE_4G_VALIDATION.md),
[ADR-0008](docs/decisions/ADR-0008-inline-value-unit-parsing.md)) implements
explicit, bounded inline value/unit cell parsing inside `datasim_pdf` on
`feat/phase-4g-inline-value-unit-parsing`. Preserve
the `datasim_intake` APIs and its candidate-only boundary. Keep PDF processing
in the isolated `datasim_pdf` adapter; do not add OCR, persistence, LLM
integration, GUI, semiconductor models, or a custom solver. Do not commit
manufacturer PDFs or copyrighted table passages to the repository. Poppler is a
GPL-2.0-or-later executable dependency; do not bundle it or infer project
redistribution rights from this ADR. The repository still has no adopted license.

Build and run CTest as documented. Integration skips are not passes. No LLM is a
numerical authority; manual RC parameters and synthetic fixtures are not
datasheet-extracted knowledge.
