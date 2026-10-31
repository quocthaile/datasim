# Agent prompt: Initialize Datasim repository documentation

Work on `https://github.com/quocthaile/datasim` in its local checkout. Datasim means
**Datasheet-driven electronic component modeling, simulation, and visualization.**

Inspect the root, default branch, working tree, recent commits, README, AGENTS.md,
source/build files, license, and conventions before editing. Preserve all files
and user changes. Create or use `docs/project-foundation`; do not commit directly
to the default branch or merge. If remote access fails, prepare local changes and
report the blocker without claiming remote success.

Reconcile README.md, AGENTS.md, docs/PROJECT_CHARTER.md, docs/ARCHITECTURE.md,
docs/COMPONENT_DATA_MODEL.md, docs/VALIDATION_STRATEGY.md, docs/ROADMAP.md,
docs/RESEARCH_REFERENCES.md, docs/REFERENCES.bib, docs/GLOSSARY.md,
docs/decisions/ADR-0001-simulation-backend-boundary.md, and this prompt. Use a
supplied foundation if available; otherwise state the baseline used.

Preserve the pipeline: Datasheet -> Document Processing -> Candidate Extracted
Data -> Validation and Engineering Review -> Component Knowledge -> Mathematical
/ Vendor Model -> Deterministic Simulation -> Interactive Visualization.

Keep evidence, observations, knowledge, models, circuit/run configurations,
results, and visualization/explanation separate. Retain min/typ/max, operating
conditions, original units/notation, source locations, extraction methods, and
review status. Separate absolute maximum ratings from recommended operation.
A datasheet parameter is not a complete mathematical model.

Allow local LLM assistance for classification, extraction, normalization,
source-grounded retrieval, and explanation. Require structured validation and
engineering review before extracted information becomes trusted simulation input.
Never make an LLM numerical authority; computation must be deterministic.

Keep a backend-independent application boundary. Record ngspice as the first
candidate only. Evaluate existing simulators before proposing a custom SPICE
solver. Do not select Qt, OpenGL, Python, SQLite, or any language in this task.

Check and annotate original references for D2S-FLOW (arXiv:2502.16540), DocEDA
(arXiv:2412.05301), Galetzka et al. (DOI:10.1002/jnm.3205), ngspice, Sandia Xyce,
KiCad SPICE, QUCS-S, UC Berkeley BSIM, IBIS 8.0, Touchstone 2.0, PlotDigitizer,
and Ollama API. Distinguish preprints, journal papers, standards, and software
documentation. Do not invent novelty claims or copy vendor models/datasheets.

Use [charter status definitions](../docs/PROJECT_CHARTER.md) consistently. Check
relative links, Mermaid syntax, citations, and cross-document consistency. Preserve
source/build configuration and add no source code or dependencies. Do not claim
unrun tests, benchmarks, simulations, or physical measurements.

Report initial state, branch, files changed, decisions, open questions, checks,
and access limitations. Include a commit SHA or PR URL only if actually created.
Stop when the documentation foundation is ready for review; do not implement the
simulation engine or any parser, UI, database, or LLM integration.
