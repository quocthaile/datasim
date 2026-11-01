# Datasim agent guidance

Read [README](README.md), [project guidance](docs/AGENTS.md), and the relevant
architecture/decision documents before changes. Phase 1 ([implementation](docs/PHASE_1_IMPLEMENTATION.md))
and Phase 1.5 ([reproducibility](docs/PHASE_1_REPRODUCIBILITY.md),
[architecture review](docs/PHASE_1_ARCHITECTURE_REVIEW.md)) are completed, merged into
`main` via PR #1 (`86b924b`), and VERIFIED on Windows x64 (MSYS2 UCRT GCC / ngspice-47)
within their ideal-RC scope. Preserve all Phase 1/1.5 behavior, the 1% step-amplitude
tolerance, and historical validation evidence, and distinguish hosted CI execution
from a local clean build.

Phase 2 is authorized and implemented on `feat/phase-2-component-model` strictly
within the scope of [Phase 2 plan](docs/PHASE_2_PLAN.md),
[Phase 2 implementation](docs/PHASE_2_IMPLEMENTATION.md),
[Phase 2 validation](docs/PHASE_2_VALIDATION.md), and
[ADR-0003](docs/decisions/ADR-0003-component-domain-model.md): in-memory C++20
component domain types (`datasim_component`), deterministic review lifecycle, model
artifact metadata contracts, referential validation, and synthetic Resistor/Capacitor
unit tests. Preserve user changes, do not commit directly to `main`, do not merge
pull requests, and do not implement PDF/OCR/datasheet ingestion, persistence, LLM
integration, GUI, or Phase 3 capabilities.

Build and run CTest as documented. Integration skips are not passes. No LLM is a
numerical authority; manual RC parameters and synthetic fixtures are not
datasheet-extracted knowledge.
