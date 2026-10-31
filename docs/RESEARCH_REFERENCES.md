# Research references

References reviewed for this foundation on 2026-10-31. Keys match
[REFERENCES.bib](REFERENCES.bib). These annotations identify relevance and limits;
they do not claim reproduction, adoption, or Datasim performance. Preprint status
describes the cited arXiv versions, not an assertion about all later publication.

## Academic work

- **`chen2025d2sflow` — preprint.** Hong Cai Chen, Yi Pin Xu, and Yang Zhang,
  *D2S-FLOW: Automated Parameter Extraction from Datasheets for SPICE Model
  Generation Using Large Language Models* (2025),
  [arXiv:2502.16540v2](https://arxiv.org/abs/2502.16540v2).
  Relevant to document-focused retrieval and terminology normalization for
  extraction and model generation. Study its evaluation assumptions before reuse;
  reported extraction results do not establish Datasim model accuracy or replace
  engineering review.
- **`chen2024doceda` — preprint.** Hong Cai Chen, Longchang Wu, Ming Gao,
  Lingrui Shen, Jiarui Zhong, and Yipin Xu, *DocEDA: Automated Extraction and Design
  of Analog Circuits from Documents with Large Language Model* (2024),
  [arXiv:2412.05301v1](https://arxiv.org/abs/2412.05301v1).
  Relevant to layout analysis, parameter extraction, and circuit-diagram processing.
  Its broader design workflow is research context, not a commitment to implement
  schematic parsing or optimization in Datasim.
- **`galetzka2024mna` — journal research article.** Armin Galetzka, Dimitrios
  Loukrezis, and Herbert De Gersem, *Data-driven model-free modified nodal analysis
  circuit solver*, International Journal of Numerical Modelling: Electronic
  Networks, Devices and Fields (2024),
  [DOI:10.1002/jnm.3205](https://doi.org/10.1002/jnm.3205).
  Studies numerical circuit solution using measurement data and circuit-law
  constraints. Useful for understanding alternatives to explicit device models;
  it is not evidence that an LLM should solve circuits or that Datasim needs a
  custom solver. The DOI landing page was inaccessible during review; title,
  authors, identifier, and subject were checked against the
  [institutionally hosted article](https://lirias.kuleuven.be/bitstream/20.500.12942/744235/2/Int%20J%20Numerical%20Modelling%20-%202024%20-%20Galetzka%20-%20Data%E2%80%90driven%20model%E2%80%90free%20modified%20nodal%20analysis%20circuit%20solver.pdf).

## Software and model documentation

- **`ngspiceDocs` — software documentation.** [ngspice documentation](https://ngspice.sourceforge.io/docs.html).
  Primary source for supported analyses, device models, execution and integration
  interfaces. First integration candidate; no dependency has been installed or
  selected. Pin a manual and release during evaluation.
- **`xyceDocs` — software documentation.** [Sandia Xyce](https://xyce.sandia.gov/).
  Alternative simulator to evaluate for required models, execution modes and
  numerical behavior. Compatibility must be demonstrated, not inferred from SPICE naming.
- **`kicadSimulation` — software documentation, version 9.0.**
  [KiCad Schematic Editor: Simulator](https://docs.kicad.org/9.0/en/eeschema/eeschema.html#simulator).
  Existing schematic/model/simulation workflow to study. Its integration choices
  do not select Datasim's UI toolkit or define a complete portable model contract.
- **`qucsSDocs` — software documentation.** [QUCS-S documentation](https://qucs-s-help.readthedocs.io/en/latest/)
  and [upstream repository](https://github.com/ra3xdh/qucs_s).
  Useful precedent for a frontend working with multiple simulation engines.
  Backend capabilities and model dialects still require explicit handling. The
  older project landing URLs could not be fetched; official documentation and
  upstream search records were used instead.
- **`berkeleyBsim` — compact-model documentation.** [UC Berkeley BSIM models](https://bsim.berkeley.edu/models/).
  Relevant to explicit model families and the distinction between physical model
  parameters and datasheet characteristics. No model family or parameter set is
  adopted here. Direct access returned 401; only the official indexed model page
  was available, so detailed manuals and licensing remain to be reviewed.
- **`plotDigitizer` — software product documentation.** [PlotDigitizer](https://plotdigitizer.com/).
  A reference for recovering samples from plots. Datasim would need to retain
  axis calibration, trace identity and digitization uncertainty. These samples
  are not new physical measurements. This citation identifies the named web
  product, not WebPlotDigitizer or other similarly named tools.
- **`ollamaApi` — API documentation.** [Ollama API introduction](https://docs.ollama.com/api/introduction).
  Candidate reference for local model serving. No runtime is selected; local
  assistance remains optional and cannot bypass validation or engineering review.

## Standards and interchange specifications

- **`ibis80` — industry specification, version 8.0.** [IBIS Version 8.0](https://ibis.org/ver8.0/).
  Reference for I/O buffer behavioral model interchange. Treat its semantics and
  validity assumptions explicitly; it is not interchangeable with a general
  transistor-level SPICE model. Version 8.0 is the requested reference, not a claim
  that it is the newest release.
- **`touchstone20` — interchange specification, version 2.0.**
  [Touchstone Version 2.0](https://ibis.org/touchstone_ver2.0/).
  Reference for network-parameter data and associated format semantics. Frequency
  domain data alone do not define arbitrary nonlinear transient behavior. Future
  support would need explicit port, unit, reference-impedance and frequency handling.

## Use and evidence policy

Link to original publications and documentation. Do not copy datasheets, vendor
models, or substantial copyrighted source material into this repository.
Before adopting an artifact, check its exact version, license, and redistribution
conditions. Citation is not permission to redistribute. Research annotations are
not a systematic novelty review; no novelty claim is made.
