# Project charter

## Mission and users

Datasim aims to help engineers trace electronic-component behavior from datasheet
evidence through reviewed knowledge and models to reproducible simulation and
understandable visualization. Intended users include engineers evaluating parts,
researchers comparing models, and learners inspecting assumptions.

## Scope

Phase 1 and Phase 1.5 update: bounded implementation of manual ideal RC
configuration, ngspice batch orchestration, analytical/common-grid validation,
JSON reporting, process hardening and Windows UCRT CI is completed, VERIFIED on
Windows x64 (MSYS2 UCRT GCC / ngspice-47), and merged into `main` via PR #1
(`86b924b4b49d69c70c1688a8748cb1c2893adc34`).
[ADR-0002](decisions/ADR-0002-phase-1-ngspice-executable.md) records those technology
choices; [validation evidence](PHASE_1_VALIDATION.md) and
[reproducibility](PHASE_1_REPRODUCIBILITY.md) cover only that ideal-RC scope.

Phase 2 update: the in-memory C++20 Component Domain Model and Knowledge Contracts
module (`datasim_component`), scoped to [PHASE_2_PLAN.md](PHASE_2_PLAN.md) and
[ADR-0003](decisions/ADR-0003-component-domain-model.md), is completed, VERIFIED on
Windows x64 (MSYS2 UCRT GCC 16.2.0), and merged into `main` via PR #2 (`b2f73d5`).
See [Phase 2 implementation](PHASE_2_IMPLEMENTATION.md) and
[Phase 2 validation](PHASE_2_VALIDATION.md).

Phase 3 update: the deterministic component-to-simulation vertical slice
(`datasim_vertical`), scoped to [PHASE_3_PLAN.md](PHASE_3_PLAN.md) and
[ADR-0004](decisions/ADR-0004-phase-3-vertical-slice.md), is implemented and
VERIFIED, merged into `main` via PR #3 (`c142a36`), on Windows x64 (MSYS2 UCRT GCC 16.2.0 /
ngspice-47). See [Phase 3 implementation](PHASE_3_IMPLEMENTATION.md) and
[Phase 3 validation](PHASE_3_VALIDATION.md). It excludes PDF/OCR parsing, persistent
storage, general-purpose JSON serialization, automatic unit conversion, LLM
integration, external SPICE subcircuits, semiconductor models, and UI. The following
paragraphs preserve the original initialization scope record.

Phase 4 update: Phase 4A planning and the Phase 4B in-memory candidate intake are
merged into `main`; Phase 4B is hosted-verified by run `41995104973`. Phase 4C
implements a separate, bounded local selectable-text PDF table adapter on its
feature branch. OCR, persistence, LLM integration, approval automation and real
datasheet accuracy claims remain out of scope or unverified; see the Phase 4C plan,
implementation and validation records.

PROPOSED capabilities include document processing, evidence-linked parameter
extraction, engineering review, component knowledge management, model association,
deterministic simulation, and interactive explanation. The immediate PLANNED
deliverable is a reviewable documentation foundation. No application capability
was IMPLEMENTED, VERIFIED, or MEASURED by the original documentation task; current
phase-specific status is recorded in the Phase 1–4 documents.

This task excludes implementation, backend installation, dependency selection,
training or integrating an LLM, UI construction, database creation, and copying
vendor assets. A custom SPICE solver is not an initial objective. Novelty and
performance claims require a separate literature review and reproducible evidence.

## Status vocabulary

These labels apply to a named capability or claim, not blanket project maturity.

| Status | Meaning and required evidence |
| --- | --- |
| PROPOSED | An idea or design under consideration; no delivery commitment or implementation implied. |
| PLANNED | Scoped work selected for a future stage with explicit exit criteria; not implemented. |
| IMPLEMENTED | An artifact or capability exists; correctness is not implied. |
| VERIFIED | A specified check actually ran and passed; retain method, inputs, versions, outcome, and scope. |
| MEASURED | A physical observation was actually acquired; retain instrument, setup, calibration, conditions, and uncertainty. |

MEASURED is an evidence category, not an automatic upgrade over VERIFIED.
Simulation and digitized datasheet curves are not new physical measurements.
Review states (`candidate`, `needs_review`, `approved`, `rejected`, `superseded`)
for extracted component data are separate from these project statuses.

## Principles and success criteria

Every trusted value must be traceable to evidence and an accountable review.
Model applicability must be explicit; simulation must retain enough configuration
to reproduce results within declared tolerances. An explanation must distinguish
source statements, modeled behavior, and assumptions. Missing information stays
missing rather than being invented.

Foundation acceptance requires consistent documents, resolving relative links,
valid Mermaid syntax, original research references, no unauthorized source/dependency
additions, and no undocumented technology choices. See
[validation](VALIDATION_STRATEGY.md).

## Historical initialization baseline

On 2026-10-31 the initial local checkout had a clean `main`, tracking `origin/main`,
with one commit (`e48c97d`, Initial commit) and only `.gitignore`. The local remote
HEAD identified `main` as default. There was no README, AGENTS.md, license, source
code, build configuration, or separate supplied project-foundation document. The
task instructions were the baseline for the initial foundation documents (`b319636`).
Subsequent Phase 1 (`021513c`) and Phase 1.5 (`99fad81`) work merged into `main` via
PR #1 at `86b924b4b49d69c70c1688a8748cb1c2893adc34`, which forms the verified
baseline for Phase 2 (`feat/phase-2-component-model`).
