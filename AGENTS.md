# Datasim agent guidance

Read [README](README.md), [project guidance](docs/AGENTS.md), and the relevant
architecture/decision documents before changes. Phase 1 authorizes only the C++20
RC/ngspice feasibility spike described in [implementation](docs/PHASE_1_IMPLEMENTATION.md).
Preserve user changes, use a feature branch, and do not merge or begin Phase 2.

Phase 1.5 authorizes bounded validation, process robustness, reproducibility and
CI hardening on `codex/phase-1-5-hardening`. Read the reproducibility and architecture
review documents. Preserve the 1% step-amplitude tolerance and distinguish hosted
CI execution from a local clean build.

Build and run CTest as documented. Integration skips are not passes. No LLM is a
numerical authority; manual RC parameters are not datasheet-extracted knowledge.
