# Glossary

| Term | Meaning in Datasim |
| --- | --- |
| Datasheet | Publisher document describing a component under stated conditions. |
| Evidence | Identifiable source content with location, revision and provenance. |
| Observation | A candidate reading from evidence, not yet trusted knowledge. |
| Component knowledge | Normalized, engineering-reviewed facts retaining source lineage. |
| Qualifier | Minimum, typical, maximum, or explicitly unspecified characterization of a value. |
| Absolute maximum rating | A stress limit; not a recommended operating point or guaranteed functional behavior. |
| Recommended operating condition | A condition intended for normal operation, distinct from stress limits. |
| Model artifact | Equations or a vendor model with explicit terminals, parameters and validity limits. |
| Validity envelope | Conditions and analyses over which a model is supported by evidence. |
| SPICE | A family of circuit simulation tools and model/netlist conventions; dialects differ. |
| Backend adapter | Boundary translating supported run specifications into a particular simulator and normalizing outputs. |
| Deterministic simulation | Numerical computation under recorded inputs and settings; reproducibility is assessed with stated tolerances. |
| MNA | Modified nodal analysis, a circuit-equation formulation. |
| BSIM | Berkeley compact transistor model family; not a model derived automatically from a datasheet. |
| IBIS | I/O Buffer Information Specification; behavioral buffer data distinct from transistor-level SPICE models. |
| Touchstone | Network-parameter interchange format; not a complete general-purpose component model. |
| Digitization | Estimating numerical samples from plotted evidence, with calibration and uncertainty. |
| Engineering review | Accountable assessment of source fidelity, physical meaning and applicability. |
| Provenance | Traceable lineage from source through transformations, decisions, models and runs. |
| LLM | Large language model; an optional assistant for candidate extraction and explanation. |

The authoritative definitions of PROPOSED, PLANNED, IMPLEMENTED, VERIFIED, and
MEASURED are in the [charter](PROJECT_CHARTER.md). Extraction review states are
defined separately in the [data model](COMPONENT_DATA_MODEL.md).
