# Project charter

## Mission and users

Datasim aims to help engineers trace electronic-component behavior from datasheet
evidence through reviewed knowledge and models to reproducible simulation and
understandable visualization. Intended users include engineers evaluating parts,
researchers comparing models, and learners inspecting assumptions.

## Scope

Phase 1 update: the user has authorized a bounded implementation of manual ideal
RC configuration, ngspice orchestration, analytical validation and export.
[ADR-0002](decisions/ADR-0002-phase-1-ngspice-executable.md) records the technology
choices and the scope change. [Validation evidence](PHASE_1_VALIDATION.md) covers
only this spike. The following paragraphs describe the original initialization scope.

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
Review states for extracted data are separate from these project statuses.

## Principles and success criteria

Every trusted value must be traceable to evidence and an accountable review.
Model applicability must be explicit; simulation must retain enough configuration
to reproduce results within declared tolerances. An explanation must distinguish
source statements, modeled behavior, and assumptions. Missing information stays
missing rather than being invented.

Foundation acceptance requires consistent documents, resolving relative links,
valid Mermaid syntax, original research references, no source/dependency additions,
and no undocumented technology choices. See [validation](VALIDATION_STRATEGY.md).

## Current baseline

On 2026-10-31 the local checkout had a clean `main`, tracking `origin/main`, with
one commit (`e48c97d`, Initial commit) and only `.gitignore`. The local remote HEAD
identified `main` as default. There was no README, AGENTS.md, license, source code,
build configuration, or separate supplied project-foundation document. The task
instructions are the baseline for these documents. Remote state could not be
confirmed during initialization; the GitHub connector returned 404 and the Git
HTTPS check could not connect.
