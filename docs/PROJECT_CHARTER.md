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

Phase 2 update: the user has authorized and `feat/phase-2-component-model`
implements the in-memory C++20 Component Domain Model and Knowledge Contracts
module (`datasim_component`), scoped strictly to [PHASE_2_PLAN.md](PHASE_2_PLAN.md)
and [ADR-0003](decisions/ADR-0003-component-domain-model.md). Local verification on
Windows x64 (MSYS2 UCRT GCC 16.2.0) is recorded in
[Phase 2 implementation](PHASE_2_IMPLEMENTATION.md) and
[Phase 2 validation](PHASE_2_VALIDATION.md). It remains independent of the ngspice
simulation backend and excludes PDF/OCR parsing, persistent storage, general-purpose
JSON serialization, automatic unit conversion, LLM integration, semiconductor
models, and UI. The following paragraphs preserve the original initialization scope
record.

PROPOSED capabilities include document processing, evidence-linked parameter
extraction, engineering review, component knowledge management, model association,
deterministic simulation, and interactive explanation. The immediate PLANNED
deliverable is a reviewable documentation foundation. No application capability
is IMPLEMENTED, VERIFIED, or MEASURED by this documentation task.

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
