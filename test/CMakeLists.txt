SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

set(TEST_OCD2_DIR "${CMAKE_CURRENT_BINARY_DIR}/testdata")
set(TEST_OCD2_FILE "${TEST_OCD2_DIR}/jieba.dict.small.ocd2")
set(BENCHMARK_OCD2_FILE "${TEST_OCD2_DIR}/jieba.dict.full.ocd2")

# rime-essay traditional dictionary (LGPL). Generated at build time into the
# build tree; never committed to this MIT repository. Path is baked into
# test_paths.h so the smoke test can find it (see the guarded target below).
set(RIME_ESSAY_SOURCE "${CMAKE_SOURCE_DIR}/third_party/rime-essay/essay.txt")
set(RIME_ESSAY_DICT_FILE "${TEST_OCD2_DIR}/jieba.rime-essay.dict.utf8")

add_custom_command(
    OUTPUT "${TEST_OCD2_FILE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TEST_OCD2_DIR}"
    COMMAND $<TARGET_FILE:cppjieba_dict>
            -i "${CMAKE_CURRENT_SOURCE_DIR}/testdata/extra_dict/jieba.dict.small.utf8"
            -i "${CMAKE_CURRENT_SOURCE_DIR}/testdata/userdict.utf8"
            -o "${TEST_OCD2_FILE}"
    DEPENDS cppjieba_dict
            "${CMAKE_CURRENT_SOURCE_DIR}/testdata/extra_dict/jieba.dict.small.utf8"
            "${CMAKE_CURRENT_SOURCE_DIR}/testdata/userdict.utf8"
    VERBATIM
)

add_custom_target(test_ocd2_dict DEPENDS "${TEST_OCD2_FILE}")

add_custom_command(
    OUTPUT "${BENCHMARK_OCD2_FILE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TEST_OCD2_DIR}"
    COMMAND $<TARGET_FILE:cppjieba_dict>
            -i "${CMAKE_SOURCE_DIR}/dict/jieba.dict.utf8"
            -i "${CMAKE_SOURCE_DIR}/dict/user.dict.utf8"
            -o "${BENCHMARK_OCD2_FILE}"
    DEPENDS cppjieba_dict
            "${CMAKE_SOURCE_DIR}/dict/jieba.dict.utf8"
            "${CMAKE_SOURCE_DIR}/dict/user.dict.utf8"
    VERBATIM
)

add_custom_target(benchmark_ocd2_dict DEPENDS "${BENCHMARK_OCD2_FILE}")

# Configure test paths
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_paths.h.in" "${CMAKE_BINARY_DIR}/test/test_paths.h")

INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/test
)

ADD_EXECUTABLE(load_test load_test.cpp)
ADD_EXECUTABLE(benchmark_bin benchmark.cpp)
target_link_libraries(load_test PRIVATE cppjieba)
target_link_libraries(benchmark_bin PRIVATE cppjieba)

# rime-essay traditional-dictionary smoke test. Only built when the LGPL
# submodule is checked out AND a Python 3 interpreter is available; otherwise it
# is skipped so a plain `git clone` (without submodules) still configures/builds.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(EXISTS "${RIME_ESSAY_SOURCE}" AND Python3_Interpreter_FOUND)
    add_custom_command(
        OUTPUT "${RIME_ESSAY_DICT_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${TEST_OCD2_DIR}"
        COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_SOURCE_DIR}/tools/convert_rime_essay.py"
                --input "${RIME_ESSAY_SOURCE}"
                --output "${RIME_ESSAY_DICT_FILE}"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/convert_rime_essay.py"
                "${RIME_ESSAY_SOURCE}"
        VERBATIM)
    add_custom_target(rime_essay_dict DEPENDS "${RIME_ESSAY_DICT_FILE}")

    add_executable(rime_essay_smoke_test rime_essay_smoke_test.cpp)
    target_link_libraries(rime_essay_smoke_test PRIVATE cppjieba)
    add_dependencies(rime_essay_smoke_test rime_essay_dict)
    add_test(NAME rime_essay_smoke_test COMMAND rime_essay_smoke_test)
else()
    message(STATUS "Skipping rime_essay_smoke_test "
                   "(submodule third_party/rime-essay or Python3 not found)")
endif()
add_dependencies(benchmark_bin benchmark_ocd2_dict)
if(WIN32)
    target_link_libraries(benchmark_bin PRIVATE Psapi)
endif()
set_target_properties(benchmark_bin PROPERTIES OUTPUT_NAME benchmark_bin)
ADD_CUSTOM_TARGET(benchmark
    COMMAND $<TARGET_FILE:benchmark_bin>
    DEPENDS benchmark_bin
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    USES_TERMINAL)
ADD_SUBDIRECTORY(unittest)

if(TARGET test.run)
    add_dependencies(test.run test_ocd2_dict)
endif()
