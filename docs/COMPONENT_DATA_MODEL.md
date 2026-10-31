# Component data model

Status: PROPOSED conceptual contracts. This document is not an executable schema,
database design, or choice of serialization format. IDs and versions below are
requirements for future design, not existing API fields.

## Entity contracts

| Entity | Required conceptual information |
| --- | --- |
| Source document / evidence | Stable ID, publisher, part identity, document title and revision, original URL, retrieval date, content hash where available, rights restrictions; page index and printed page label, section, table/figure, cell or region and relevant footnotes. |
| Parameter observation | ID, source/evidence references, original parameter name and notation, original value and units, minimum/typical/maximum qualifier or explicitly unspecified, conditions, rating category, extraction method/version, extraction time, uncertainty, and review status/history. |
| Component knowledge | Component identity including manufacturer, part suffix and package; versioned normalized facts, canonical quantity/unit, explicit conversion, observation references, applicable conditions, conflicts, reviewer and decision rationale. |
| Model artifact | ID/version/hash, format or equations, vendor/author, source and license, terminals/pin mapping, parameters and knowledge references, calibration/validation evidence, assumptions, supported analyses/backends, validity envelope and limitations. |
| Circuit / run configuration | Topology, node and pin mappings, exact model versions/hashes, component values, stimuli, temperature, initial conditions, analysis and sweep settings, solver tolerances/options, backend/version, platform and random seeds if relevant. |
| Simulation result | Run/configuration reference, execution manifest, completion status, independent axes and units, dependent arrays and units, diagnostics, convergence warnings, timestamps, backend output reference/hash, and provenance. |
| Visualization / explanation | Result and evidence references, view configuration, units, transformations/interpolation, annotations, assumptions, generation method/version, and review history where applicable. |

## Preservation and normalization

Keep minimum, typical, and maximum as distinct observations or explicitly labeled
values under the same conditions. A blank datasheet cell means absent, not zero.
Preserve inequalities, ranges, symbols, tolerances, test circuits, pulse duration,
temperature, frequency, and footnotes. Unit conversion creates a derived value
with a documented rule; it never replaces the original notation.

Use distinct categories for absolute maximum rating, recommended operating
condition, electrical characteristic, and unspecified. Absolute maxima are stress
limits, not operating targets or guaranteed model parameters. Never infer category
or model applicability from the parameter name alone.

Curve observations additionally need figure location, axis units, linear/log scale,
calibration points, trace/legend identity, digitization method, and uncertainty.
Keep curve samples separate from fitted model coefficients. Typical curves do not
establish worst-case bounds. Datasheet observations are published evidence, not
measurements performed by Datasim.

## Promotion and revision

Proposed observation review states are `candidate`, `needs_review`, `approved`,
`rejected`, and `superseded`. They are distinct from project capability statuses.
Candidates pass structured checks and then an identified engineer reviews evidence,
units, qualifiers, conditions, category, and intended use. Only approved observations
may contribute to trusted knowledge. Approval of a fact does not validate a model.

Retain competing observations rather than silently averaging conflicts. Record
reviewer, date, rationale, and previous version for every promotion or correction.
Changed evidence invalidates affected approvals until reviewed; downstream models
and runs retain their original version references and must be flagged for reassessment.
Missing conditions or contradictory source values block promotion for simulation use.

## Illustrative lineage

A hypothetical table entry `10 mA`, qualified as typical at `25 degC`, remains a
typical observation at that condition. Normalization may derive `0.010 A` while
retaining the original text and table location. It cannot become a maximum current
rating or a complete device model. Even after review, a separate model decision
must establish what equation, terminal, and validity range can use that value.
This example is synthetic and describes no measured device.

See [validation](VALIDATION_STRATEGY.md) and [architecture](ARCHITECTURE.md).
