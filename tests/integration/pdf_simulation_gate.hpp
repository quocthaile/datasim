#pragma once

#include "datasim/vertical/assembly/circuit_assembly.hpp"
#include <filesystem>

namespace datasim::vertical::testing {

// Constructs the Phase 4D PDF-derived RC CircuitAssembly by running real Poppler extraction
// on the synthetic datasheet PDF fixture, validating intake manifests, applying synthetic
// engineering review, normalizing condition-compatible (T_A=85 degC) R and C parameters,
// preserving conflicts and rejected observations, and binding validated synthetic models.
[[nodiscard]] CircuitAssembly make_pdf_extracted_rc_assembly(
    const std::filesystem::path& pdf_path,
    const std::filesystem::path& pdfinfo_path,
    const std::filesystem::path& pdftotext_path);

// Executes the full Phase 4D PDF-to-Simulation end-to-end validation gate, including
// positive Poppler -> intake -> review -> knowledge -> model -> ngspice execution,
// manual RcConfig numerical and byte-level equivalence, provenance verification,
// and all negative trust-boundary and determinism tests.
void run_pdf_to_simulation_gate(
    const std::filesystem::path& ngspice_exe,
    const std::filesystem::path& workspace_dir);

} // namespace datasim::vertical::testing

