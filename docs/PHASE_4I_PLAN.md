# Phase 4I plan: Baseline-aware text reconstruction

Status: IMPLEMENTED; bounded verification is recorded in
[PHASE_4I_VALIDATION.md](PHASE_4I_VALIDATION.md).

Prerequisite verified: Phase 4H merged through PR #12 at
`864eb56c1712470180cc1ecb9d9dbef3b5fd05ff`, with successful post-merge Windows
[CI 42058944651](https://github.com/quocthaile/datasim/actions/runs/42058944651).
Branch: `feat/phase-4i-baseline-aware-text`, created from refreshed main.

Implement only explicit host/script associations under
[ADR-0010](decisions/ADR-0010-baseline-aware-text-reconstruction.md), authored
before implementation. Preserve original word geometry and text, candidate-only
output, global row tolerance, strict numerical grammar, and all earlier fixtures.
Reject ambiguous hosts, conflicting ownership, header/footnote interference, and
uncertain source geometry. No automatic typography recognition is authorized.

Acceptance: new fictional selectable-text cases; Release-active positive and
negative checks; provenance tampering tests; default compatibility against actual
Phase 4H executable output; complete CTest and real-ngspice study; unchanged-label
local corpus comparisons with source reconstruction and fail-closed controls.
Keep manufacturer source text and annotations local and preserve old snapshots.

Open a separate PR and stop at review. Phase 4J may begin only after this phase is
merged into main and hosted post-merge CI succeeds. No automatic merge is allowed.
