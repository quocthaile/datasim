# Validation strategy

Status: PROPOSED engineering validation program. No extraction benchmark,
simulation, accuracy experiment, or physical measurement has been run for this
foundation. Documentation checks do not establish numerical correctness.

Phase 1 update: numerical RC verification and automated tests have now run; see
[Phase 1 validation](PHASE_1_VALIDATION.md) for exact scope, results and limitations.
Physical validation and the remaining gates below remain PROPOSED.

## Gates

| Gate | Proposed checks | Evidence needed to pass |
| --- | --- | --- |
| Source integrity | Identity, revision, hash, readable location, rights and footnotes. | Traceable source manifest and permitted evidence access. |
| Extraction structure | Required fields, numeric syntax, units, qualifiers, categories, conditions and source references. | Machine-check results; ambiguous/missing values remain blocked. |
| Engineering review | Compare original source; assess plausibility, conflicts, operating envelope and intended use. | Named reviewer, decision, date and rationale; no automatic LLM approval. |
| Model suitability | Equations/artifact, pins, dimensions, parameter mapping, validity range, license and backend support. | Independent model review and documented limitations. |
| Numerical verification | Analytic cases, reference simulations, convergence and sensitivity checks. | Reproducible inputs, versions, settings, tolerances and actual outcomes. |
| Physical validation | Compare predictions against controlled physical observations. | Instrument/setup/calibration, raw observations, conditions and uncertainty. |
| Presentation integrity | Axis units/scales, transformations, missing data, warnings and explanation citations. | Comparison against raw results and evidence. |

## Future test design

Start with licensed or synthetic fixtures. Include missing units, OCR confusion,
reversed min/max, conditions in footnotes, mixed revisions, absolute ratings,
conflicting values, and logarithmic curves. Assess value, unit, qualifier,
condition, and provenance accuracy separately against engineer-reviewed labels.
Avoid a single score that hides errors in conditions or source identity.

Candidate numerical cases include a resistor divider, RC transient, and a reviewed
nonlinear device model. Define expected behavior and absolute/relative tolerances
before running them. Compare analytic solutions where available; cross-backend
agreement is supporting evidence rather than proof. Test unsupported models,
non-convergence, invalid topology, and out-of-envelope use as visible failures.

Pin backend/model versions, input hashes, options, seeds, platform, and analysis
settings. Deterministic computation does not promise bitwise equality across
platforms; report observed reproducibility within declared tolerances. Separate
model error, extraction error, numerical error, and measurement uncertainty.

## Documentation acceptance

Check all local Markdown targets and anchors, fenced-block balance, Mermaid parse
validity, bibliography keys and original identifiers, status terminology, and
cross-document architectural agreement. Inspect Git status and diff for unintended
source or configuration changes. Report exact checks and unavailable checks in
the completion report. Do not add a build or test framework for this documentation
task. Use [charter statuses](PROJECT_CHARTER.md) only for the scope actually checked.
