# Agent instructions

## Scope

Read [README.md](../README.md), the [charter](PROJECT_CHARTER.md), and relevant
decision records before edits. The documentation-only initialization task, the
Phase 1 C++20/CMake RC feasibility spike ([ADR-0002](decisions/ADR-0002-phase-1-ngspice-executable.md)),
and Phase 1.5 numerical/process/CI hardening are completed and merged into `main`
at `86b924b4b49d69c70c1688a8748cb1c2893adc34` (PR #1).

Phase 2 ([PHASE_2_PLAN.md](PHASE_2_PLAN.md),
[PHASE_2_IMPLEMENTATION.md](PHASE_2_IMPLEMENTATION.md),
[ADR-0003](decisions/ADR-0003-component-domain-model.md)) is completed and merged
into `main` at `b2f73d5` (PR #2).

Phase 3 authorizes the deterministic component-to-simulation vertical slice
(`datasim_vertical`) defined in [PHASE_3_PLAN.md](PHASE_3_PLAN.md),
[PHASE_3_IMPLEMENTATION.md](PHASE_3_IMPLEMENTATION.md),
[PHASE_3_VALIDATION.md](PHASE_3_VALIDATION.md), and
[ADR-0004](decisions/ADR-0004-phase-3-vertical-slice.md), implemented on
`feat/phase-3-vertical-slice`, merged into `main` at
`6516a629c75662b83f48704bfd7978bcc299ff78` (hosted Windows CI run `41284202716`), and
hardened via PR #3 (merge `c142a36`). Keep `datasim_component`'s
single-component boundary and `datasim`'s ideal-RC solver behavior unchanged. Do not
implement PDF parsing, OCR, datasheet ingestion, UI, database/persistent storage,
general-purpose JSON serialization, automatic unit conversion, LLM/Ollama integration,
semiconductor simulation, or a custom solver.

Phase 4A and Phase 4B are merged into `main` by PRs #5 and #6; Phase 4B's P-07
real-ngspice release gate passed hosted Windows CI run `41995104973`. Phase 4C
([PHASE_4C_PLAN.md](PHASE_4C_PLAN.md), [PHASE_4C_IMPLEMENTATION.md](PHASE_4C_IMPLEMENTATION.md),
[PHASE_4C_VALIDATION.md](PHASE_4C_VALIDATION.md), and
[ADR-0006](decisions/ADR-0006-deterministic-pdf-table-extraction.md)) is implemented,
merged into `main` via PR #7 (`1704f726d901f3e1d00f9cdc918e80d352b443ac`), and
verified on synthetic selectable-text PDF fixtures by hosted Windows CI runs
`42000652152` (PR #7 branch) and `42017272043` (post-merge `main`). Phase 4D
([PHASE_4D_PLAN.md](PHASE_4D_PLAN.md), [PHASE_4D_IMPLEMENTATION.md](PHASE_4D_IMPLEMENTATION.md),
[PHASE_4D_VALIDATION.md](PHASE_4D_VALIDATION.md)) implements the PDF-to-Simulation
end-to-end validation gate, merged into `main` via PR #8
(`69aba4df7ff752355a40e394de5d05ac988df34b`) and verified by hosted Windows CI
run `42025406013`. Phase 4E ([PHASE_4E_PLAN.md](PHASE_4E_PLAN.md),
[PHASE_4E_CORPUS_PROTOCOL.md](PHASE_4E_CORPUS_PROTOCOL.md),
[PHASE_4E_VALIDATION.md](PHASE_4E_VALIDATION.md)) implements the offline
real-datasheet evaluation corpus and benchmark, merged into `main` via PR #9
(`b45c32224e0316dd9f93b6e9bfc3ab89cd3ceaf4`) and verified by hosted Windows CI
run `42037281172`. Phase 4F ([PHASE_4F_PLAN.md](PHASE_4F_PLAN.md),
[PHASE_4F_IMPLEMENTATION.md](PHASE_4F_IMPLEMENTATION.md),
[PHASE_4F_VALIDATION.md](PHASE_4F_VALIDATION.md),
[ADR-0007](decisions/ADR-0007-explicit-header-binding.md)) implements explicit,
source-verified column-header unit and condition bindings inside `datasim_pdf`,
merged into `main` via PR #10 (`9f3c115d13a40204d3d7f83382a50c107b14b803`) and
verified by hosted Windows CI run `42044651168`. Phase 4G
([PHASE_4G_PLAN.md](PHASE_4G_PLAN.md),
[PHASE_4G_IMPLEMENTATION.md](PHASE_4G_IMPLEMENTATION.md),
[PHASE_4G_VALIDATION.md](PHASE_4G_VALIDATION.md),
[ADR-0008](decisions/ADR-0008-inline-value-unit-parsing.md)) implements explicit,
bounded inline value/unit cell parsing inside `datasim_pdf` on
`feat/phase-4g-inline-value-unit-parsing`. No physical measurement is claimed.
Keep the Poppler adapter in `datasim_pdf`, above and depending on `datasim_intake`;
do not modify existing Phase 2/3/4B/4D public contracts without a demonstrated,
documented correctness defect. Do not infer licensing rights or commit vendor PDFs.

## Working practices

- Inspect Git status, branches, recent commits, and existing files before editing.
- Preserve user changes and historical branches/evidence. Use a feature/fix branch
  branched from `origin/main`; do not
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
Run the CMake build and CTest suite documented in
[Phase 1](PHASE_1_IMPLEMENTATION.md) and [Phase 2](PHASE_2_IMPLEMENTATION.md).
Report unavailable tools or remote access honestly; skipped integration tests are not passes.

Phase 4G is merged via PR #11 at `357dc52aa809`, with successful post-merge
Windows CI run `42052973122`. Phase 4H is implemented on
`feat/phase-4h-hierarchical-table-context`; see [PHASE_4H_VALIDATION.md](PHASE_4H_VALIDATION.md)
and [ADR-0009](decisions/ADR-0009-hierarchical-table-context.md).
Preserve explicit parent/child source verification, candidate-only output, and
legacy extraction with no hierarchy bindings. Phase 4I may start only after Phase
4H merges and post-merge CI succeeds. Do not merge automatically.

Phase 4H is merged via PR #12 at `864eb56c1712470180cc1ecb9d9dbef3b5fd05ff`
and hosted-verified by post-merge Windows run `42058944651`. Phase 4I is merged
via PR #13 at `fb35ae1fc5a58dcd6b9f89d7a6807c3198ab99a5` and hosted-verified by
post-merge Windows run `42065262568`; see [validation](PHASE_4I_VALIDATION.md)
and [ADR-0010](decisions/ADR-0010-baseline-aware-text-reconstruction.md).
Do not change global row tolerance or numeric grammar to interpret typography.
Phase 4J is limited to final validation, release documentation, and the isolated
temporary-workspace safety fix recorded in
[PHASE_4_FINAL_VALIDATION.md](PHASE_4_FINAL_VALIDATION.md). Hosted run `42148338325` passed; validation is VERIFIED. Require successful
hosted branch CI for any later code changes. Do not implement parser features or
merge automatically.
