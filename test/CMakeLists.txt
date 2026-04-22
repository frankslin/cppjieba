SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

set(TEST_OCD2_DIR "${CMAKE_CURRENT_BINARY_DIR}/testdata")
set(TEST_OCD2_FILE "${TEST_OCD2_DIR}/jieba.dict.small.ocd2")

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
if(WIN32)
    target_link_libraries(benchmark_bin Psapi)
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
