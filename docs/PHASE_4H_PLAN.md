# Phase 4H: Hierarchical table context

Status: IMPLEMENTED; verification evidence is in [validation](PHASE_4H_VALIDATION.md).
Branch: `feat/phase-4h-hierarchical-table-context`.

Baseline: refreshed `origin/main` at Phase 4G merge `357dc52aa809`, with successful
post-merge [Windows run 42052973122](https://github.com/quocthaile/datasim/actions/runs/42052973122).
The starting working tree was clean. No main commit or automatic merge is allowed.

Implement only the explicit parent-row and bounded merged-parameter-cell context
specified by [ADR-0009](decisions/ADR-0009-hierarchical-table-context.md), written
before source changes. Reuse the actual extraction entry point, candidate intake,
physical row IDs, and existing evidence references. Add no numerical grammar,
baseline reconstruction, downstream API change, or automatic engineering action.

Acceptance covers two parent groups, bounded child selection, exact source text,
overlap/conflict rejection, complete labels, merged cells, multiple-MPN negative
controls, default compatibility, deterministic output, and separate provenance.
Public tests use a new fictional selectable-text fixture; existing PDF fixture
bytes and all prior benchmark files remain unchanged.

Run a clean Windows UCRT Release build, all CTest suites with zero mandatory skips,
CI runtime tests, and the six-run RC study. Run the established evaluator on the
unchanged local Phase 4G 15- and 22-observation annotation sets, keeping comparisons
separate. Preserve baseline snapshots and report actual denominators, false accepts,
parameter-label agreement, and evidence checks. No accuracy guarantee is assumed.

Open one PR against main and stop for review. Phase 4I must wait for this PR's merge
and successful post-merge CI; Phase 4J remains gated on Phase 4I.
