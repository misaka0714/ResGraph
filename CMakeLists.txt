cmake_minimum_required(VERSION 3.16)

project(
  resgraph_algorithm
  VERSION 0.1.0
  DESCRIPTION "Cross-model graph ANN index migration"
  LANGUAGES CXX)

option(RESGRAPH_BUILD_EXAMPLES "Build the minimal ResGraph example" ON)
option(RESGRAPH_BUILD_TESTS "Build ResGraph tests" ON)

find_package(Threads REQUIRED)

add_library(
  resgraph
  src/embedding_space.cpp
  src/graph.cpp
  src/local_migration.cpp
  src/neighbor_selection.cpp
  src/packed_signatures.cpp
  src/search_migration.cpp)
add_library(resgraph::resgraph ALIAS resgraph)

target_compile_features(resgraph PUBLIC cxx_std_17)
target_include_directories(
  resgraph
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(resgraph PUBLIC Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(resgraph PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(RESGRAPH_BUILD_EXAMPLES)
  add_executable(resgraph_minimal examples/minimal.cpp)
  target_link_libraries(resgraph_minimal PRIVATE resgraph::resgraph)
endif()

if(RESGRAPH_BUILD_TESTS)
  enable_testing()
  add_executable(resgraph_tests tests/resgraph_test.cpp)
  target_link_libraries(resgraph_tests PRIVATE resgraph::resgraph)
  add_test(NAME resgraph_tests COMMAND resgraph_tests)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
  TARGETS resgraph
  EXPORT resgraphTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT resgraphTargets
  NAMESPACE resgraph::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/resgraph)

configure_package_config_file(
  cmake/resgraphConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/resgraphConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/resgraph)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/resgraphConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/resgraphConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/resgraphConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/resgraph)
install(FILES LICENSE NOTICE README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
