# Roadmap

These stages are ordered by dependency, not promised delivery dates.

Phase 1 and Phase 1.5 foundation: manual ideal-RC transient orchestration,
executable ngspice-47 integration, common-grid validation across four timestep
refinements, characteristic checkpoints, repeatability, JSON reports, and process
fault hardening are completed, merged into `main` via PR #1
(`86b924b4b49d69c70c1688a8748cb1c2893adc34`), and **VERIFIED** locally on Windows
x64 (MSYS2 UCRT GCC 16.2.0 / ngspice-47) and in hosted Windows CI by
[run 41217248663](https://github.com/quocthaile/datasim/actions/runs/41217248663)
(3/3 tests with zero skips, real integration, and six-run study). POSIX and MSVC
builds remain unverified. See [implementation](PHASE_1_IMPLEMENTATION.md),
[evidence](PHASE_1_VALIDATION.md), [reproducibility](PHASE_1_REPRODUCIBILITY.md),
and [architecture review](PHASE_1_ARCHITECTURE_REVIEW.md).

Phase 2 (**Component Domain Model & Knowledge Contracts**, implementing the
in-memory C++20 contracts of Stage 3 below) is completed, merged into `main` via
PR #2 (`b2f73d5`), and **VERIFIED** on Windows x64 (MSYS2 UCRT GCC 16.2.0): 4/4 CTest
suites passed with zero skips, including 270 component domain checks on synthetic
Resistor and Capacitor fixtures alongside unchanged Phase 1/1.5 unit, fault, and
real ngspice-47 integration tests. See [Phase 2 plan](PHASE_2_PLAN.md),
[Phase 2 implementation](PHASE_2_IMPLEMENTATION.md),
[Phase 2 validation](PHASE_2_VALIDATION.md), and
[ADR-0003](decisions/ADR-0003-component-domain-model.md).

Phase 3 (**Deterministic Component-to-Simulation Vertical Slice**, implementing the
ideal-RC slice of Stage 4 below) is **IMPLEMENTED and VERIFIED** on `main` at
`6516a629c75662b83f48704bfd7978bcc299ff78` in hosted Windows CI
([run 41284202716](https://github.com/quocthaile/datasim/actions/runs/41284202716):
5/5 CTest suites with 0 skips, 1392 vertical-slice unit checks, real `ngspice-47`
integration, and six-run study) and further hardened via PR #3
(merge `c142a36`, hosted CI run 41766031217 `success`) (1483 vertical-slice unit checks plus byte-identical
real `ngspice-47` integration against manual `RcConfig`). See
[Phase 3 plan](PHASE_3_PLAN.md), [Phase 3 implementation](PHASE_3_IMPLEMENTATION.md),
[Phase 3 validation](PHASE_3_VALIDATION.md), and
[ADR-0004](decisions/ADR-0004-phase-3-vertical-slice.md). Persistent storage,
serialization, datasheet parsing, LLM integration, external SPICE subcircuits,
nonlinear semiconductor models, physical measurement validation (`MEASURED`), and
Stage 5+ remain unauthorized/`PROPOSED`.

Phase 4A planning and Phase 4B candidate intake are **MERGED** into `main` via
normal-merge PRs #5 and #6. Phase 4B is **VERIFIED** by hosted Windows CI run
[41995104973](https://github.com/quocthaile/datasim/actions/runs/41995104973),
including its non-skipped P-07 ngspice path and convergence/repeatability check.
The merged main commit is `7d9af1c88929fe3c29d55e1c2438e8d2041acb2a`; its push
workflow is separately tracked as run
[41995458137](https://github.com/quocthaile/datasim/actions/runs/41995458137).

Phase 4C's bounded deterministic PDF table adapter is **IMPLEMENTED** on
`feat/phase-4c-deterministic-extraction`. It uses Poppler `26.10.0` executables,
explicit PDF/table selection, candidate-only intake, and fictional fixtures. Local
Release CTest passed 7/7 non-skipped suites, while real ngspice remained skipped
locally. Phase 4C hosted verification and PR review are pending. See
[Phase 4C plan](PHASE_4C_PLAN.md), [implementation](PHASE_4C_IMPLEMENTATION.md),
[validation](PHASE_4C_VALIDATION.md), and
[ADR-0006](decisions/ADR-0006-deterministic-pdf-table-extraction.md).

| Stage | Status | Scope and exit criteria |
| --- | --- | --- |
| 0: Foundation | IMPLEMENTED | Foundation committed at b319636; documentation available for review. |
| 1: Engineering scope | PROPOSED | Select an initial component family, analyses, licensed fixtures, and review responsibilities; define acceptance tolerances. (Phase 1/1.5 verified only manual ideal-RC transient feasibility.) |
| 2: Backend evaluation | PROPOSED | Evaluate ngspice first and compare alternatives such as Xyce; record compatibility, licensing, embedding/process options, and reproducible reference cases in an ADR. (ADR-0002 selected ngspice executable integration for Phase 1 ideal RC only.) |
| 3: Knowledge contracts | VERIFIED (in-memory C++20; persistence PROPOSED) | In-memory C++20 evidence, observation, deterministic review lifecycle, component knowledge, conflict visibility, and model metadata/validation contracts verified on synthetic Resistor and Capacitor fixtures ([Phase 2 validation](PHASE_2_VALIDATION.md)). Persistent storage, interchange serialization, and curve series remain PROPOSED. |
| 4: Deterministic vertical slice | VERIFIED (ideal-RC transient slice; general multi-analysis/subcircuit PROPOSED) | Approved `ComponentKnowledge` and explicitly selected `ModelArtifactDescriptor` instances connected via `CircuitAssembly`, SI parameter resolution, model/backend compatibility gate, `RcConfig` adapter, and `provenance_metadata.json` ([Phase 3 validation](PHASE_3_VALIDATION.md)). External subcircuits and nonlinear device models remain PROPOSED. |
| 5: Evidence intake and bounded extraction | Phase 4B VERIFIED; Phase 4C IMPLEMENTED on feature branch | Candidate-only in-memory intake is merged and hosted-verified. The selected-PDF table adapter is under review; OCR, arbitrary layout parsing, LLM assistance, and persistent storage remain PROPOSED. |
| 6: Visualization and validation | PROPOSED | Present results and evidence with units, warnings and assumptions; conduct separately scoped physical validation if equipment/data are available. |

## Phase 4C limits and follow-on decisions

The implemented PDF adapter is a bounded table-extraction MVP, not arbitrary PDF
table recognition. It does not provide OCR, persistence, publisher authentication,
licensing conclusions, automatic engineering interpretation, or approval. Poppler
is an external GPL-2.0-or-later executable dependency; the project license and any
binary redistribution decision remain open. No real-datasheet accuracy is claimed
without a labeled, legally usable corpus.

## Open technical questions

- Which device family, model fidelity, and DC/AC/transient analyses come first for Stage 4?
- Which datasheets and vendor models permit the intended use and distribution?
- Who owns engineering review, model approval, and later correction handling in multi-user workflows?
- What reference circuits and tolerances define sufficient numerical accuracy beyond ideal RC?
- Which ngspice integration mode and alternatives meet portability and licensing needs?
- How will backend-specific models, multi-pin bonded terminals, unsupported analyses, and version changes be persisted and interchanged?
- Which UI/graphics framework, persistent storage engine, unit-algebra design, and packaging choices fit those requirements?
- Is local LLM assistance beneficial on the selected corpus, and which runtime/model fits?
- What repository license should the owner adopt?

Phase 1/1.5 answers the initial ideal-RC/transient scope ([ADR-0002](decisions/ADR-0002-phase-1-ngspice-executable.md))
and Phase 2 establishes the dependency-free C++20 in-memory component domain contracts
([ADR-0003](decisions/ADR-0003-component-domain-model.md)). No choice of Qt, OpenGL,
Python runtime dependency, SQLite, JSON library, or custom SPICE solver is made.
