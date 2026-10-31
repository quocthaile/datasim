# Agent instructions

## Scope

Read [README.md](../README.md), the [charter](PROJECT_CHARTER.md), and relevant
decision records before edits. The documentation-only initialization task, the
Phase 1 C++20/CMake RC feasibility spike ([ADR-0002](decisions/ADR-0002-phase-1-ngspice-executable.md)),
and Phase 1.5 numerical/process/CI hardening are completed and merged into `main`
at `86b924b4b49d69c70c1688a8748cb1c2893adc34` (PR #1).

Phase 2 authorizes only the in-memory C++20 Component Domain Model and Knowledge
Contracts defined in [PHASE_2_PLAN.md](PHASE_2_PLAN.md) and
[ADR-0003](decisions/ADR-0003-component-domain-model.md) on branch
`feat/phase-2-component-model`. Keep Phase 2 independent of the existing ngspice
simulation implementation. Do not implement PDF parsing, OCR, datasheet ingestion,
UI, database/persistent storage, general-purpose JSON serialization, automatic unit
conversion, LLM/Ollama integration, semiconductor simulation, or a custom solver;
do not start Phase 3.

## Working practices

- Inspect Git status, branches, recent commits, and existing files before editing.
- Preserve user changes and historical branches/evidence. Use
  `feat/phase-2-component-model` branched from `origin/main` for Phase 2; do not
  commit directly to `main` or merge pull requests.
- Keep documentation links relative within the repository. Use stable original
  URLs and identifiers for research, with matching BibTeX keys.
- Distinguish proposals from implementation using the charter's five statuses.
- Never claim a build, benchmark, simulation, or accuracy result was VERIFIED or
  MEASURED unless the corresponding check or physical measurement actually ran.
- Record commands, scope, outcomes, and limitations when reporting checks.
- Do not copy copyrighted datasheets or vendor models into the repository.
- Do not infer a technology choice from `.gitignore`. Record future choices in ADRs.

## Engineering boundaries

Keep source evidence, observations, normalized knowledge, model artifacts,
circuit/run configuration, results, and visual explanations separate. Preserve
minimum/typical/maximum qualifiers, units, original notation, operating conditions,
source location, extraction method, and review status. Never substitute absolute
maximum ratings for recommended operating conditions.

All extracted information needs structured validation and engineering review
before it becomes trusted simulation input. LLM output is candidate information,
not numerical authority. Simulation must use deterministic computation through a
backend-independent boundary. Evaluate existing simulators before proposing a
custom SPICE solver; ngspice executable integration is selected only for Phase 1.

## Documentation checks

Check relative links, Mermaid syntax, citation identifiers, status language, and
agreement between the charter, architecture, model, roadmap, and ADR. Confirm the
diff contains only authorized changes and existing user work is preserved.
Run the CMake build and CTest suite documented in [Phase 1](PHASE_1_IMPLEMENTATION.md).
Report unavailable tools or remote access honestly; skipped integration tests are not passes.
