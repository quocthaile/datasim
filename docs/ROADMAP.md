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

Phase 2 (**Component Domain Model & Knowledge Contracts**, implementing the in-memory
C++20 contracts of Stage 3 below) is **PLANNED** and authorized on
`feat/phase-2-component-model` strictly within [PHASE_2_PLAN.md](PHASE_2_PLAN.md)
and [ADR-0003](decisions/ADR-0003-component-domain-model.md). Later stages,
persistence, datasheet parsing, LLM integration, and GUI remain unauthorized.

| Stage | Status | Scope and exit criteria |
| --- | --- | --- |
| 0: Foundation | IMPLEMENTED | Foundation committed at b319636; documentation available for review. |
| 1: Engineering scope | PROPOSED | Select an initial component family, analyses, licensed fixtures, and review responsibilities; define acceptance tolerances. (Phase 1/1.5 verified only manual ideal-RC transient feasibility.) |
| 2: Backend evaluation | PROPOSED | Evaluate ngspice first and compare alternatives such as Xyce; record compatibility, licensing, embedding/process options, and reproducible reference cases in an ADR. (ADR-0002 selected ngspice executable integration for Phase 1 ideal RC only.) |
| 3: Knowledge contracts | PLANNED | Specify and implement in-memory C++20 evidence, observation, review lifecycle, knowledge and model metadata contracts with deterministic validation on synthetic Resistor/Capacitor fixtures ([Phase 2 plan](PHASE_2_PLAN.md)). Persistence and serialization remain deferred. |
| 4: Deterministic vertical slice | PROPOSED | Implement only after separate authorization: approved knowledge, explicit model, run configuration, backend adapter, and traceable results with numerical checks. |
| 5: Assisted extraction | PROPOSED | Evaluate document processing and optional local LLM assistance against reviewed fixtures; prove the review gate blocks invalid inputs. |
| 6: Visualization and validation | PROPOSED | Present results and evidence with units, warnings and assumptions; conduct separately scoped physical validation if equipment/data are available. |

## Open technical questions

- Which device family, model fidelity, and DC/AC/transient analyses come first?
- Which datasheets and vendor models permit the intended use and distribution?
- Who owns engineering review, model approval, and later correction handling?
- What reference circuits and tolerances define sufficient numerical accuracy?
- Which ngspice integration mode and alternatives meet portability and licensing needs?
- How will backend-specific models, unsupported analyses, and version changes be represented?
- Which language, UI/graphics framework, storage, and packaging choices fit those requirements?
- Is local LLM assistance beneficial on the selected corpus, and which runtime/model fits?
- What repository license should the owner adopt?

Phase 1 answers the initial ideal-RC/transient scope and selects C++20, CMake and
executable integration in ADR-0002. The broader questions remain open. No choice
of Qt, OpenGL, Python, SQLite, or other product infrastructure is made. A custom solver
would require a demonstrated unmet need after evaluating existing tools.
