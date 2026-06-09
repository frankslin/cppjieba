CMAKE_MINIMUM_REQUIRED (VERSION 3.10)
PROJECT(CPPJIEBA)

set(ENABLE_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/deps/marisa-trie EXCLUDE_FROM_ALL)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/deps/darts-clone/include
  ${PROJECT_SOURCE_DIR}/deps/marisa-trie/include)

if(NOT DEFINED CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 17)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

ADD_DEFINITIONS(-O3 -g)

# Define a variable to check if this is the top-level project
if(NOT DEFINED CPPJIEBA_TOP_LEVEL_PROJECT)
    if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(CPPJIEBA_TOP_LEVEL_PROJECT ON)
    else()
        set(CPPJIEBA_TOP_LEVEL_PROJECT OFF)
    endif()
endif()

if(NOT TARGET cppjieba)
  add_library(cppjieba INTERFACE)
  target_include_directories(cppjieba INTERFACE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/deps/darts-clone/include
    ${PROJECT_SOURCE_DIR}/deps/marisa-trie/include
  )
  target_link_libraries(cppjieba INTERFACE Marisa::marisa)
endif()

include(GNUInstallDirs)
install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY dict/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/cppjieba/dict)

if(CPPJIEBA_TOP_LEVEL_PROJECT)
  ENABLE_TESTING()

  add_executable(cppjieba_dict tools/cppjieba_dict.cpp)
  target_link_libraries(cppjieba_dict PRIVATE Marisa::marisa)

  message(STATUS "MSVC value: ${MSVC}")
  ADD_SUBDIRECTORY(test)
  ADD_TEST(NAME ./test/test.run COMMAND ./test/test.run)
  ADD_TEST(NAME ./load_test COMMAND ./load_test)
endif()
