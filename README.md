# Datasim

**Datasheet-driven electronic component modeling, simulation, and visualization.**

Datasim is at the documentation-foundation stage. The application, parser,
simulation integration, and visualization are **PROPOSED**, not implemented.
There is no build system or runnable application yet.

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
- [Agent instructions](AGENTS.md) and [repository initialization prompt](prompts/AGENT_INIT_REPOSITORY.md)

## Contributing at this stage

Keep changes documentation-first. Preserve evidence, units, operating conditions,
and review history. Record technology choices in decision records before adopting
them. ngspice is the first integration candidate, not an installed dependency.
Language, UI toolkit, graphics stack, storage, and LLM runtime remain undecided.

The existing `.gitignore` mentions native build tools; it does not select them.
No license file is present. Project licensing remains an owner decision; do not
assume redistribution rights for this repository or third-party assets. Link to
datasheets and vendor models instead of copying them into the repository.
