# Datasim

**Datasheet-driven electronic component modeling, simulation, and visualization.**

Datasim has a **VERIFIED Phase 1 feasibility spike** for an ideal RC transient:
C++20 orchestration, ngspice batch execution, CSV export, and independent analytical
validation. This is not a production simulator. Datasheet extraction, component
knowledge management, and interactive visualization remain **PROPOSED**.

See [build/run instructions](docs/PHASE_1_IMPLEMENTATION.md) and
[actual validation evidence](docs/PHASE_1_VALIDATION.md). A C++20 compiler and
CMake build the application; an external ngspice executable runs integration tests.

Phase 1.5 hardening adds common-grid convergence/checkpoint validation, repeat-run
evidence, JSON reports and timeout/concurrency tests. Local verification and
[hosted Windows CI](https://github.com/quocthaile/datasim/actions/runs/41217248663)
passed, including real ngspice integration and all six study runs. See
[reproducibility](docs/PHASE_1_REPRODUCIBILITY.md) and the
[architecture review](docs/PHASE_1_ARCHITECTURE_REVIEW.md) for verification scope
and remaining limitations. No Phase 2 capability is implemented.

The intended workflow moves from source evidence to reviewed component knowledge,
then to an explicitly selected model and deterministic simulation. A datasheet
parameter is not automatically a complete mathematical model. Local LLMs may
assist document work and explanations; they are never the numerical authority.

## Documentation

- [Project charter and status definitions](docs/PROJECT_CHARTER.md)
- [Architecture and trust boundaries](docs/ARCHITECTURE.md)
- [Component data model](docs/COMPONENT_DATA_MODEL.md)
- [Validation strategy](docs/VALIDATION_STRATEGY.md)
- [Roadmap and open questions](docs/ROADMAP.md)
- [Annotated research references](docs/RESEARCH_REFERENCES.md) and [BibTeX](docs/REFERENCES.bib)
- [Glossary](docs/GLOSSARY.md)
- [ADR-0001: simulation backend boundary](docs/decisions/ADR-0001-simulation-backend-boundary.md)
- [Agent instructions](AGENTS.md) and [repository initialization prompt](docs/AGENT_INIT_REPOSITORY.md)
- [ADR-0002: Phase 1 executable integration](docs/decisions/ADR-0002-phase-1-ngspice-executable.md)

## Contributing at this stage

Keep changes scoped to the current phase. Preserve evidence, units, operating conditions,
and review history. Record technology choices in decision records before adopting
them. Phase 1 selects C++20, CMake/CTest and ngspice executable integration.
UI toolkit, graphics stack, storage, and LLM runtime remain undecided.

The original `.gitignore` is unchanged; generated runs and local tools belong under `build/`.
No license file is present. Project licensing remains an owner decision; do not
assume redistribution rights for this repository or third-party assets. Link to
datasheets and vendor models instead of copying them into the repository.
