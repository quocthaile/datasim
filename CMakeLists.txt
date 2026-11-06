cmake_minimum_required(VERSION 3.20)
project(Datasim VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Identity covers production source, application and test inputs, CMake
# templates, CI workflows and synthetic fixture data. It is a reproducibility
# fingerprint, not a signature or authentication mechanism.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(COMMAND git describe --always --dirty WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE DATASIM_GIT_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT DATASIM_GIT_REVISION)
    set(DATASIM_GIT_REVISION "source-export")
endif()
file(GLOB_RECURSE identity_files CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    src/* apps/* tests/* cmake/* scripts/ci/* .github/workflows/* assets/*)
list(APPEND identity_files CMakeLists.txt)
list(SORT identity_files)
set(identity "")
foreach(path IN LISTS identity_files)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
    if(path MATCHES "(^|/)(__pycache__|CMakeFiles|\.pytest_cache)(/|$)|(^|/)[^/]+(~|\.tmp|\.pyc)$")
        continue()
    endif()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${path}")
        continue()
    endif()
    if(path MATCHES "\\.(pdf|png|jpg|jpeg|gif|bin)$")
        if(NOT path MATCHES "^tests/fixtures/(image_only_datasheet|synthetic_datasheet|synthetic_header_binding_datasheet|synthetic_inline_unit_datasheet|synthetic_hierarchy_datasheet|synthetic_baseline_datasheet)\\.pdf$")
            continue()
        endif()
        file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${path}" digest)
    else()
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${path}" contents)
        string(REPLACE "\r\n" "\n" contents "${contents}")
        string(SHA256 digest "${contents}")
    endif()
    string(APPEND identity "${path}:${digest}\n")
endforeach()
string(SHA256 DATASIM_SOURCE_SHA256 "${identity}")
configure_file(cmake/build_info.hpp.in generated/datasim/build_info.hpp @ONLY)
add_library(datasim
    src/datasim/simulation/circuit/rc.cpp
    src/datasim/simulation/analysis/reference.cpp
    src/datasim/simulation/analysis/grid.cpp
    src/datasim/simulation/validation/validation.cpp
    src/datasim/simulation/io/samples.cpp
    src/datasim/simulation/io/report.cpp
    src/datasim/simulation/backend/process.cpp
    src/datasim/simulation/backend/ngspice.cpp)
target_include_directories(datasim PUBLIC src PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_library(datasim_component
    src/datasim/component/observation/review.cpp
    src/datasim/component/knowledge/component_knowledge.cpp
    src/datasim/component/model/model_validation.cpp
    src/datasim/component/validation/domain_validation.cpp)
target_include_directories(datasim_component PUBLIC src)
add_library(datasim_vertical
    src/datasim/vertical/assembly/circuit_assembly.cpp
    src/datasim/vertical/resolution/parameter_resolution.cpp
    src/datasim/vertical/compatibility/model_compatibility.cpp
    src/datasim/vertical/adapter/rc_simulation_adapter.cpp
    src/datasim/vertical/provenance/run_provenance.cpp)
target_include_directories(datasim_vertical PUBLIC src PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(datasim_vertical PUBLIC datasim_component datasim)
# Phase 4B evidence intake: depends only on datasim_component (no simulation, no vertical slice).
add_library(datasim_intake
    src/datasim/intake/numeric/raw_numeric.cpp
    src/datasim/intake/validation/intake_validation.cpp
    src/datasim/intake/builder/candidate_bundle.cpp)
target_include_directories(datasim_intake PUBLIC src)
target_link_libraries(datasim_intake PUBLIC datasim_component)
# Phase 4C PDF adapter is isolated above the stable intake contracts.
add_library(datasim_pdf
    src/datasim/pdf/process_capture.cpp
    src/datasim/pdf/table_extraction.cpp)
target_include_directories(datasim_pdf PUBLIC src PRIVATE src/datasim/pdf)
target_link_libraries(datasim_pdf PUBLIC datasim_intake)
if(MSVC)
    target_compile_options(datasim PRIVATE /W4 /permissive-)
    target_compile_options(datasim_component PRIVATE /W4 /permissive-)
    target_compile_options(datasim_vertical PRIVATE /W4 /permissive-)
    target_compile_options(datasim_intake PRIVATE /W4 /permissive-)
    target_compile_options(datasim_pdf PRIVATE /W4 /permissive-)
else()
    target_compile_options(datasim PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(datasim_component PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(datasim_vertical PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(datasim_intake PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(datasim_pdf PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(rc_demo apps/rc_demo/main.cpp)
target_link_libraries(rc_demo PRIVATE datasim)
add_executable(rc_study apps/rc_study/main.cpp)
target_link_libraries(rc_study PRIVATE datasim)
add_executable(datasheet_extract apps/datasheet_extract/main.cpp)
target_link_libraries(datasheet_extract PRIVATE datasim_pdf)
add_executable(datasheet_eval_dump apps/datasheet_eval_dump/main.cpp)
target_link_libraries(datasheet_eval_dump PRIVATE datasim_pdf)
if(MSVC)
    target_compile_options(datasheet_eval_dump PRIVATE /W4 /permissive-)
else()
    target_compile_options(datasheet_eval_dump PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(DATASIM_BUILD_DESKTOP "Build the optional read-only desktop waveform viewer" OFF)
option(DATASIM_BUILD_PDF_WORKBENCH "Build the optional Qt PDF datasheet evidence workbench" OFF)
if(DATASIM_BUILD_PDF_WORKBENCH AND NOT DATASIM_BUILD_DESKTOP)
    message(FATAL_ERROR "DATASIM_BUILD_PDF_WORKBENCH=ON requires DATASIM_BUILD_DESKTOP=ON")
endif()
if(DATASIM_BUILD_DESKTOP)
    if(DATASIM_BUILD_PDF_WORKBENCH)
        find_package(Qt6 6.11 REQUIRED COMPONENTS Widgets Pdf PdfWidgets Svg)
        foreach(required_target Qt6::Pdf Qt6::PdfWidgets Qt6::Svg)
            if(NOT TARGET ${required_target})
                message(FATAL_ERROR "Qt PDF workbench requires CMake target ${required_target} (Qt 6.11.x)")
            endif()
        endforeach()
    else()
        find_package(Qt6 6.11 REQUIRED COMPONENTS Widgets)
    endif()
    add_executable(datasim_viewer
        apps/datasim_viewer/main.cpp
        apps/datasim_viewer/read_only_samples.cpp
        apps/datasim_viewer/waveform_widget.cpp
        apps/datasim_viewer/workbench_service.cpp
        apps/datasim_viewer/simulation_worker.cpp
        apps/datasim_viewer/simulation_worker.hpp
        apps/datasim_viewer/csv_load_worker.cpp
        apps/datasim_viewer/csv_load_worker.hpp)
    if(DATASIM_BUILD_PDF_WORKBENCH)
        set(CMAKE_AUTORCC ON)
        target_sources(datasim_viewer PRIVATE
            apps/datasim_viewer/evidence_workbench.cpp
            apps/datasim_viewer/evidence_workbench.hpp
            apps/datasim_viewer/extraction_worker.cpp
            apps/datasim_viewer/extraction_worker.hpp
            apps/datasim_viewer/review_session.cpp
            apps/datasim_viewer/review_session.hpp
            assets/viewer.qrc)
        set_target_properties(datasim_viewer PROPERTIES AUTORCC ON)
        target_link_libraries(datasim_viewer PRIVATE datasim_pdf Qt6::Pdf Qt6::PdfWidgets Qt6::Svg)
        target_compile_definitions(datasim_viewer PRIVATE DATASIM_PDF_WORKBENCH=1)
    endif()
    set_target_properties(datasim_viewer PROPERTIES AUTOMOC ON)
    target_include_directories(datasim_viewer PRIVATE apps/datasim_viewer)
    target_link_libraries(datasim_viewer PRIVATE datasim Qt6::Widgets)
    if(MSVC)
        target_compile_options(datasim_viewer PRIVATE /W4 /permissive-)
    else()
        target_compile_options(datasim_viewer PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(unit_tests tests/unit/main.cpp)
    target_link_libraries(unit_tests PRIVATE datasim)
    add_test(NAME unit COMMAND unit_tests)
    add_executable(component_tests
        tests/unit/component_fixtures.cpp
        tests/unit/component_domain_tests.cpp)
    target_link_libraries(component_tests PRIVATE datasim_component)
    if(MSVC)
        target_compile_options(component_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(component_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME component_model COMMAND component_tests)
    add_executable(vertical_slice_tests
        tests/unit/component_fixtures.cpp
        tests/unit/vertical_slice_fixtures.cpp
        tests/unit/vertical_slice_tests.cpp)
    target_link_libraries(vertical_slice_tests PRIVATE datasim_vertical)
    if(MSVC)
        target_compile_options(vertical_slice_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(vertical_slice_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME vertical_slice COMMAND vertical_slice_tests)
    add_executable(intake_tests
        tests/unit/intake_fixtures.cpp
        tests/unit/intake_tests.cpp)
    target_link_libraries(intake_tests PRIVATE datasim_intake)
    add_executable(intake_handoff_tests
        tests/unit/component_fixtures.cpp
        tests/unit/vertical_slice_fixtures.cpp
        tests/unit/intake_fixtures.cpp
        tests/unit/intake_handoff_tests.cpp)
    target_link_libraries(intake_handoff_tests PRIVATE datasim_intake datasim_vertical)
    if(MSVC)
        target_compile_options(intake_tests PRIVATE /W4 /permissive-)
        target_compile_options(intake_handoff_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(intake_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(intake_handoff_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME intake COMMAND intake_tests)
    add_test(NAME intake_handoff COMMAND intake_handoff_tests)
    add_executable(pdf_extraction_tests tests/unit/pdf_extraction_tests.cpp)
    target_link_libraries(pdf_extraction_tests PRIVATE datasim_pdf)
    target_compile_definitions(pdf_extraction_tests PRIVATE
        DATASIM_SYNTHETIC_PDF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/synthetic_datasheet.pdf"
        DATASIM_IMAGE_ONLY_PDF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/image_only_datasheet.pdf"
        DATASIM_HEADER_BINDING_PDF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/synthetic_header_binding_datasheet.pdf"
        DATASIM_INLINE_UNIT_PDF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/synthetic_inline_unit_datasheet.pdf"
        DATASIM_HIERARCHY_PDF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/synthetic_hierarchy_datasheet.pdf"
        DATASIM_BASELINE_PDF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/synthetic_baseline_datasheet.pdf")
    if(MSVC)
        target_compile_options(pdf_extraction_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(pdf_extraction_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME pdf_extraction COMMAND pdf_extraction_tests)
    add_executable(pdf_process_fixture tests/unit/pdf_process_fixture.cpp)
    add_executable(pdf_process_tests tests/unit/pdf_process_tests.cpp src/datasim/pdf/process_capture.cpp)
    target_include_directories(pdf_process_tests PRIVATE src/datasim/pdf)
    add_test(NAME pdf_process COMMAND pdf_process_tests $<TARGET_FILE:pdf_process_fixture>
        "${CMAKE_CURRENT_BINARY_DIR}/pdf-process-tests")
    set_tests_properties(pdf_process PROPERTIES LABELS unit TIMEOUT 15)
    add_executable(pdf_coordinate_tests tests/unit/pdf_coordinate_tests.cpp)
    target_include_directories(pdf_coordinate_tests PRIVATE src/datasim/pdf)
    target_link_libraries(pdf_coordinate_tests PRIVATE datasim_pdf)
    add_test(NAME pdf_coordinates COMMAND pdf_coordinate_tests)
    set_tests_properties(pdf_coordinates PROPERTIES LABELS unit TIMEOUT 15)
    set(DATASIM_PDF_TEST_TEMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/pdf-extraction-temp")
    file(MAKE_DIRECTORY "${DATASIM_PDF_TEST_TEMP_DIR}")
    set_tests_properties(pdf_extraction PROPERTIES
        LABELS integration
        TIMEOUT 30
        ENVIRONMENT "TMP=${DATASIM_PDF_TEST_TEMP_DIR};TEMP=${DATASIM_PDF_TEST_TEMP_DIR};DATASIM_PDF_TEST_TEMP_ROOT=${DATASIM_PDF_TEST_TEMP_DIR}")
    add_executable(backend_fixture tests/unit/backend_fixture.cpp)
    add_executable(backend_error_tests tests/unit/backend_errors.cpp)
    target_link_libraries(backend_error_tests PRIVATE datasim)
    add_test(NAME backend_errors COMMAND backend_error_tests $<TARGET_FILE:backend_fixture>)
    add_executable(integration_tests
        tests/unit/component_fixtures.cpp
        tests/unit/vertical_slice_fixtures.cpp
        tests/unit/intake_fixtures.cpp
        tests/integration/intake_integration_fixture.cpp
        tests/integration/pdf_simulation_gate.cpp
        tests/integration/main.cpp)
    target_link_libraries(integration_tests PRIVATE datasim_vertical datasim_intake datasim_pdf)
    target_compile_definitions(integration_tests PRIVATE
        DATASIM_SYNTHETIC_PDF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/synthetic_datasheet.pdf"
        DATASIM_IMAGE_ONLY_PDF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/image_only_datasheet.pdf")
    if(MSVC)
        target_compile_options(integration_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(integration_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    set(DATASIM_NGSPICE "" CACHE FILEPATH "Optional path to ngspice executable")
    add_test(NAME ngspice_rc COMMAND integration_tests "${DATASIM_NGSPICE}")
    set_tests_properties(ngspice_rc PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 45)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(
        NAME pdf_corpus_evaluation
        COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation" -v)
    set_tests_properties(
        pdf_corpus_evaluation
        PROPERTIES
            LABELS integration
            TIMEOUT 45
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;DATASIM_EVAL_DUMP_BIN=$<TARGET_FILE:datasheet_eval_dump>;DATASIM_REPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}")
    set_tests_properties(unit component_model vertical_slice intake intake_handoff backend_errors PROPERTIES LABELS unit TIMEOUT 15)
    set_tests_properties(ngspice_rc PROPERTIES LABELS integration)
    if(DATASIM_BUILD_DESKTOP)
        add_executable(viewer_sample_tests
            apps/datasim_viewer/read_only_samples.cpp
            tests/desktop/viewer_sample_tests.cpp)
        target_include_directories(viewer_sample_tests PRIVATE apps/datasim_viewer)
        target_link_libraries(viewer_sample_tests PRIVATE datasim)
        add_test(NAME viewer_sample_loading COMMAND viewer_sample_tests "${CMAKE_CURRENT_BINARY_DIR}")
        set_tests_properties(viewer_sample_loading PROPERTIES LABELS desktop TIMEOUT 15)
        add_executable(viewer_workbench_tests
            apps/datasim_viewer/workbench_service.cpp
            apps/datasim_viewer/read_only_samples.cpp
            tests/desktop/viewer_workbench_tests.cpp)
        target_include_directories(viewer_workbench_tests PRIVATE apps/datasim_viewer)
        target_link_libraries(viewer_workbench_tests PRIVATE datasim)
        if(MSVC)
            target_compile_options(viewer_workbench_tests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(viewer_workbench_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME viewer_workbench COMMAND viewer_workbench_tests
            "${DATASIM_NGSPICE}" "${CMAKE_CURRENT_BINARY_DIR}/viewer-workbench-tests")
        set_tests_properties(viewer_workbench PROPERTIES LABELS desktop TIMEOUT 90 SKIP_RETURN_CODE 77)
        add_executable(viewer_worker_tests
            apps/datasim_viewer/workbench_service.cpp
            apps/datasim_viewer/simulation_worker.cpp
            apps/datasim_viewer/simulation_worker.hpp
            tests/desktop/viewer_worker_tests.cpp)
        set_target_properties(viewer_worker_tests PROPERTIES AUTOMOC ON)
        target_include_directories(viewer_worker_tests PRIVATE apps/datasim_viewer)
        target_link_libraries(viewer_worker_tests PRIVATE datasim Qt6::Core)
        if(MSVC)
            target_compile_options(viewer_worker_tests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(viewer_worker_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME viewer_worker COMMAND viewer_worker_tests
            "${DATASIM_NGSPICE}" "${CMAKE_CURRENT_BINARY_DIR}/viewer-worker-tests")
        set_tests_properties(viewer_worker PROPERTIES LABELS desktop TIMEOUT 60 SKIP_RETURN_CODE 77)
        if(DATASIM_BUILD_PDF_WORKBENCH)
            add_executable(evidence_workbench_tests
                apps/datasim_viewer/evidence_workbench.cpp
                apps/datasim_viewer/evidence_workbench.hpp
                apps/datasim_viewer/extraction_worker.cpp
                apps/datasim_viewer/extraction_worker.hpp
                apps/datasim_viewer/review_session.cpp
                apps/datasim_viewer/review_session.hpp
                tests/desktop/evidence_workbench_tests.cpp
                assets/viewer.qrc)
            set_target_properties(evidence_workbench_tests PROPERTIES AUTOMOC ON AUTORCC ON)
            target_include_directories(evidence_workbench_tests PRIVATE apps/datasim_viewer)
            target_link_libraries(evidence_workbench_tests PRIVATE datasim_pdf Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Svg)
            target_compile_definitions(evidence_workbench_tests PRIVATE
                DATASIM_SYNTHETIC_PDF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/synthetic_datasheet.pdf"
                DATASIM_IMAGE_ONLY_PDF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/image_only_datasheet.pdf")
            add_test(NAME evidence_workbench COMMAND evidence_workbench_tests)
            set_tests_properties(evidence_workbench PROPERTIES LABELS desktop TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        endif()
    endif()
endif()
