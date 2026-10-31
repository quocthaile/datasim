# ADR-0001: Simulation backend boundary

Date: 2026-10-31

Status: PROPOSED implementation; architecture constraint recorded for foundation review.

## Context

Datasim needs traceable numerical results while processing uncertain document
observations. Binding all application concepts to one simulator would impede
evaluation and make backend-specific assumptions difficult to identify. Writing a
new SPICE solver would introduce substantial numerical work before requirements
and existing alternatives have been evaluated.

## Decision

Keep the application independent of a particular simulation backend through an
explicit adapter boundary. A run specification identifies reviewed inputs, model
versions, topology, analyses, conditions, and solver controls. An adapter reports
capabilities, validates compatibility, executes deterministic computation, and
returns arrays with units plus diagnostics and a reproducibility manifest.

Record ngspice as the first integration candidate, not an implemented or installed
dependency. Evaluate existing simulators, including Xyce, and inspect KiCad and
QUCS-S workflows before proposing a custom SPICE solver. No final backend,
integration mechanism, programming language, UI toolkit, or storage choice is made.

LLMs may assist document work and explanation, but must not be the numerical
authority. Structured validation and engineering review precede use of extracted
information as trusted simulation input. Models need their own suitability checks.

## Alternatives and consequences

Direct coupling to ngspice could simplify an early experiment but would leak model
and execution assumptions into the application. A custom solver offers control but
requires separate justification and extensive numerical verification. An adapter
adds translation work and must expose unsupported capabilities and semantic
differences rather than promise universal model portability.

## Follow-up and reconsideration

Evaluate licensing, model/analysis support, platform availability, batch/library
interfaces, diagnostics, resource isolation, and reproducibility using explicit
reference circuits. Record actual results before selecting a backend. Revisit
the boundary if demonstrated requirements cannot be represented faithfully;
record a superseding ADR instead of silently changing this decision.

Related: [architecture](../ARCHITECTURE.md), [validation](../VALIDATION_STRATEGY.md),
[research](../RESEARCH_REFERENCES.md), [roadmap](../ROADMAP.md).
