cmake_minimum_required(VERSION 3.20)
project(Datasim VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Identity includes all build/source/test inputs, even in an exported source tree.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(COMMAND git describe --always --dirty WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE DATASIM_GIT_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT DATASIM_GIT_REVISION)
    set(DATASIM_GIT_REVISION "source-export")
endif()
file(GLOB_RECURSE identity_files CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    src/*.cpp src/*.hpp apps/*.cpp tests/*.cpp cmake/*.in)
list(APPEND identity_files CMakeLists.txt)
list(SORT identity_files)
set(identity "")
foreach(path IN LISTS identity_files)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
    file(READ "${path}" contents)
    string(REPLACE "\r\n" "\n" contents "${contents}")
    string(SHA256 digest "${contents}")
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
if(MSVC)
    target_compile_options(datasim PRIVATE /W4 /permissive-)
    target_compile_options(datasim_component PRIVATE /W4 /permissive-)
    target_compile_options(datasim_vertical PRIVATE /W4 /permissive-)
else()
    target_compile_options(datasim PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(datasim_component PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(datasim_vertical PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(rc_demo apps/rc_demo/main.cpp)
target_link_libraries(rc_demo PRIVATE datasim)
add_executable(rc_study apps/rc_study/main.cpp)
target_link_libraries(rc_study PRIVATE datasim)
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
    add_executable(backend_fixture tests/unit/backend_fixture.cpp)
    add_executable(backend_error_tests tests/unit/backend_errors.cpp)
    target_link_libraries(backend_error_tests PRIVATE datasim)
    add_test(NAME backend_errors COMMAND backend_error_tests $<TARGET_FILE:backend_fixture>)
    add_executable(integration_tests
        tests/unit/component_fixtures.cpp
        tests/unit/vertical_slice_fixtures.cpp
        tests/integration/main.cpp)
    target_link_libraries(integration_tests PRIVATE datasim_vertical)
    set(DATASIM_NGSPICE "" CACHE FILEPATH "Optional path to ngspice executable")
    add_test(NAME ngspice_rc COMMAND integration_tests "${DATASIM_NGSPICE}")
    set_tests_properties(ngspice_rc PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 45)
    set_tests_properties(unit component_model vertical_slice backend_errors PROPERTIES LABELS unit TIMEOUT 15)
    set_tests_properties(ngspice_rc PROPERTIES LABELS integration)
endif()
