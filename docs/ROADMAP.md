# Roadmap

These stages are ordered by dependency, not promised delivery dates.

Phase 1.5 hardening: numerical/common-grid validation, four timestep refinements,
characteristic checkpoints, repeatability, JSON reports and expanded failure tests
are VERIFIED locally on Windows/GCC/ngspice-47. The fresh-checkout build and all
three tests passed; local Phase 1.5 exit criteria are satisfied. Hosted Windows CI
is VERIFIED by [run 41217248663](https://github.com/quocthaile/datasim/actions/runs/41217248663):
build, 3/3 tests with zero skips, real integration and the six-run study passed
after correcting ngspice runtime paths. See [reproducibility](PHASE_1_REPRODUCIBILITY.md)
and [architecture review](PHASE_1_ARCHITECTURE_REVIEW.md). Do not begin Phase 2.

The separately authorized **Phase 1 RC feasibility spike is VERIFIED** on the
documented Windows/GCC/ngspice environment: manual ideal components, executable
adapter, CSV retrieval and analytical validation. See [implementation](PHASE_1_IMPLEMENTATION.md)
and [evidence](PHASE_1_VALIDATION.md). This does not complete the broader knowledge
contracts or reviewed-datasheet vertical slice below, and Phase 2 has not begun.

| Stage | Status | Scope and exit criteria |
| --- | --- | --- |
| 0: Foundation | IMPLEMENTED | Foundation committed at b319636; documentation available for review. |
| 1: Engineering scope | PROPOSED | Select an initial component family, analyses, licensed fixtures, and review responsibilities; define acceptance tolerances. |
| 2: Backend evaluation | PROPOSED | Evaluate ngspice first and compare alternatives such as Xyce; record compatibility, licensing, embedding/process options, and reproducible reference cases in an ADR. |
| 3: Knowledge contracts | PROPOSED | Specify evidence, observation, knowledge and model schemas with revision rules; demonstrate review on synthetic or permitted examples. |
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
