# Agent instructions

## Scope

Read [README.md](../README.md), the [charter](PROJECT_CHARTER.md), and relevant
decision records before edits. The documentation-only initialization task is
complete. The user's Phase 1 request authorizes C++20/CMake RC orchestration,
ngspice batch integration, validation, export and tests, as recorded in
[ADR-0002](decisions/ADR-0002-phase-1-ngspice-executable.md). Do not implement
datasheet parsing, UI, database, LLM integration, or a custom solver; do not start Phase 2.

## Working practices

- Inspect Git status, branches, recent commits, and existing files before editing.
- Preserve user changes. Use `feat/phase-1-ngspice-rc` for Phase 1; do not commit
  directly to the default branch or merge the work.
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
