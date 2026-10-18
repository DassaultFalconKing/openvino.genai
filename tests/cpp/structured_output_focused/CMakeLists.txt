cmake_minimum_required(VERSION 3.20)
project(genai_structured_output_focused LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(GENAI_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../..")
get_filename_component(GENAI_SOURCE_DIR "${GENAI_SOURCE_DIR}" ABSOLUTE)
set(XGRAMMAR_SOURCE_DIR "" CACHE PATH "Fresh XGrammar source tree")
set(MINJA_SOURCE_DIR "" CACHE PATH "Minja headers, commit 3e4c61c616eda133cfb1e440fc7a14bf1729bbee")
set(GGUF_SOURCE_DIR "" CACHE PATH "gguf-tools headers, commit bac796ada809ac293e685db59b075971181cb008")
set(OPENVINO_RUNTIME_DIR "" CACHE PATH "OpenVINO runtime with include and lib directories")
set(NLOHMANN_INCLUDE_DIR "" CACHE PATH "Directory containing nlohmann/json.hpp")
file(WRITE "${CMAKE_BINARY_DIR}/config.cmake"
    "set(XGRAMMAR_BUILD_PYTHON_BINDINGS OFF)\nset(XGRAMMAR_BUILD_CXX_TESTS OFF)\nset(XGRAMMAR_ENABLE_CPPTRACE OFF)\n")
add_subdirectory("${XGRAMMAR_SOURCE_DIR}" xgrammar EXCLUDE_FROM_ALL)
add_subdirectory("${XGRAMMAR_SOURCE_DIR}/3rdparty/googletest" googletest EXCLUDE_FROM_ALL)
# GenerationConfig's tokenizer-aware overload refers to this accessor. Compile
# its exact production definition without pulling in model loading and tokenizer
# conversion. Backend matching itself uses the vocabulary constructor.
set(tokenizer_impl "${GENAI_SOURCE_DIR}/src/cpp/src/tokenizer/tokenizer_impl.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${tokenizer_impl}")
file(READ "${tokenizer_impl}" tokenizer_source)
string(FIND "${tokenizer_source}" "std::shared_ptr<StructuredOutputController> Tokenizer::TokenizerImpl::get_structured_output_controller(" accessor_start)
string(FIND "${tokenizer_source}" "} // namespace genai" accessor_end REVERSE)
if(accessor_start LESS 0 OR accessor_end LESS accessor_start)
    message(FATAL_ERROR "Cannot locate production tokenizer controller accessor")
endif()
math(EXPR accessor_length "${accessor_end} - ${accessor_start}")
string(SUBSTRING "${tokenizer_source}" ${accessor_start} ${accessor_length} accessor_source)
string(SHA256 accessor_sha256 "${accessor_source}")
if(NOT accessor_sha256 STREQUAL "b58833b5190c5e1fc6c0b077b1ec8b0bf43984966a966497d08153a20af2f573")
    message(FATAL_ERROR "Production tokenizer accessor changed; review focused slice before updating hash")
endif()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/tokenizer_controller_accessor.cpp" CONTENT
    "#include \"sampling/structured_output/structured_output_controller.hpp\"\nnamespace ov { namespace genai {\n${accessor_source}\n} }\n")
add_executable(genai_structured_output
    "${GENAI_SOURCE_DIR}/tests/cpp/test_structured_output_json_schema.cpp"
    "${GENAI_SOURCE_DIR}/tests/cpp/test_structured_output_token_bridge.cpp"
    "${GENAI_SOURCE_DIR}/src/cpp/src/sampling/structured_output/xgrammar_backend.cpp"
    "${GENAI_SOURCE_DIR}/src/cpp/src/sampling/structured_output/structured_output_controller.cpp"
    "${GENAI_SOURCE_DIR}/src/cpp/src/generation_config.cpp"
    "${CMAKE_BINARY_DIR}/tokenizer_controller_accessor.cpp"
    "${GENAI_SOURCE_DIR}/src/cpp/src/logger.cpp")
# Build the selected implementation into this executable. DLL export annotations
# would retain every unrelated pipeline vtable pulled in by GenAI's headers.
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/static-include/openvino/genai/visibility.hpp" CONTENT
    "#pragma once\n#define OPENVINO_GENAI_EXPORTS\n")
target_compile_definitions(genai_structured_output PRIVATE OPENVINO_GENAI_XGRAMMAR_TESTS)
target_include_directories(genai_structured_output PRIVATE
    "${CMAKE_BINARY_DIR}/static-include"
    "${GENAI_SOURCE_DIR}/src/cpp/include" "${GENAI_SOURCE_DIR}/src/cpp/src"
    "${OPENVINO_RUNTIME_DIR}/include" "${MINJA_SOURCE_DIR}/include"
    "${GGUF_SOURCE_DIR}" "${NLOHMANN_INCLUDE_DIR}")
target_link_libraries(genai_structured_output PRIVATE xgrammar gtest_main
    "${OPENVINO_RUNTIME_DIR}/lib/intel64/Release/openvino.lib")
set(GENAI_EXTRA_XGRAMMAR_TEST_SOURCE "" CACHE FILEPATH "Optional narrow XGrammar regression source")
if(GENAI_EXTRA_XGRAMMAR_TEST_SOURCE)
    target_sources(genai_structured_output PRIVATE "${GENAI_EXTRA_XGRAMMAR_TEST_SOURCE}")
endif()
if(MSVC)
    target_compile_options(genai_structured_output PRIVATE /Gy /bigobj)
    target_link_options(genai_structured_output PRIVATE /OPT:REF /OPT:ICF)
endif()
enable_testing()
add_test(NAME structured_output COMMAND genai_structured_output)

# Optional compile-only probes for changed binding translation units. These do
# not link, import, or establish Python/Node addon runtime compatibility.
option(GENAI_FOCUSED_BINDING_PROBES "Compile changed Python and Node binding TUs" OFF)
set(PYTHON_INCLUDE_DIR "" CACHE PATH "Python development headers")
set(BINDING_DEPS_DIR "${CMAKE_BINARY_DIR}/deps" CACHE PATH "Pinned pybind11 and Node API header checkouts")
if(GENAI_FOCUSED_BINDING_PROBES)
    add_library(genai_binding_probes OBJECT
        "${GENAI_SOURCE_DIR}/src/python/py_generation_config.cpp"
        "${GENAI_SOURCE_DIR}/src/python/py_utils.cpp"
        "${GENAI_SOURCE_DIR}/src/js/src/helper.cpp")
    target_include_directories(genai_binding_probes PRIVATE
        $<TARGET_PROPERTY:genai_structured_output,INCLUDE_DIRECTORIES>
        "${GENAI_SOURCE_DIR}/src" "${GENAI_SOURCE_DIR}/src/js"
        "${BINDING_DEPS_DIR}/pybind11/include" "${PYTHON_INCLUDE_DIR}"
        "${BINDING_DEPS_DIR}/node-addon-api" "${BINDING_DEPS_DIR}/node-api-headers/include")
    target_compile_definitions(genai_binding_probes PRIVATE NAPI_VERSION=8)
    if(MSVC)
        target_compile_options(genai_binding_probes PRIVATE /bigobj)
    endif()
endif()
