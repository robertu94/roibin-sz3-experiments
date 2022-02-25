## template for a CMake C++ Library
# vim: ft=cmake :
cmake_minimum_required(VERSION 3.14)
project(roibin_test VERSION "0.0.1" LANGUAGES CXX C)

#correct was to set a default build type
# https://blog.kitware.com/cmake-and-the-default-build-type/
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type was set. Setting build type to ${default_build_type}.")
  set(CMAKE_BUILD_TYPE ${default_build_type} CACHE 
    STRING "Choose the type to build" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

include(GNUInstallDirs)
include(CheckCXXCompilerFlag)

#compiler flags and standard conformance checks
check_cxx_compiler_flag("-fno-omit-frame-pointer" HAVE_NO_OMIT_FRAME_POINTER)
set(NO_OMIT_FRAME_POINTER_FLAG "")
if(HAVE_NO_OMIT_FRAME_POINTER)
  set(NO_OMIT_FRAME_POINTER_FLAG "-fno-omit-frame-pointer")
endif()


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(std_compat REQUIRED)
find_package(LibPressio REQUIRED)
find_package(MPI REQUIRED COMPONENTS CXX)
find_package(HDF5 REQUIRED COMPONENTS C)

add_library(roibin_helpers
  ./src/hdf5_helpers.cc
  ./src/debug_helpers.cc
  )
target_compile_options(roibin_helpers PUBLIC 
  $<$<CONFIG:Debug>: -Wall -Werror -Wextra -Wpedantic>
  $<$<CONFIG:RelWithDebInfo>: ${NO_OMIT_FRAME_POINTER_FLAG}>
  )
target_include_directories(roibin_helpers
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  )
target_link_libraries(roibin_helpers
  PUBLIC
  LibPressio::libpressio
  MPI::MPI_CXX
  hdf5::hdf5
  )
target_compile_features(roibin_helpers PUBLIC cxx_std_17)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/roibin_test_version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/roibin_test_version.h
  )

add_executable(partition ./src/partition.cc)
target_link_libraries(partition PRIVATE roibin_helpers)

add_executable(extract_subset ./src/extract_subset.cc)
target_link_libraries(extract_subset PRIVATE roibin_helpers)

add_executable(pressio_load ./src/pressio_load.cc)
target_link_libraries(pressio_load PRIVATE roibin_helpers)

add_executable(roibin_test ./src/roibin_test.cc)
target_link_libraries(roibin_test PRIVATE roibin_helpers)

install(TARGETS roibin_test EXPORT roibin_test
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )


option(USE_CLANG_TIDY "include clang-tidy warnings in the build log" OFF)
if(USE_CLANG_TIDY)
  find_program(CLANG_TIDY clang-tidy)
  set_target_properties(roibin_test PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY}")
  set_target_properties(roibin_test PROPERTIES C_CLANG_TIDY "${CLANG_TIDY}")
endif()

option(USE_INCLUDE_WHAT_YOU_USE "include include-what-you-use warnings in the build log" OFF)
if(USE_INCLUDE_WHAT_YOU_USE)
  find_program(INCLUDE_WHAT_YOU_USE include-what-you-use)
  set_target_properties(roibin_test PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "${INCLUDE_WHAT_YOU_USE}")
  set_target_properties(roibin_test PROPERTIES C_INCLUDE_WHAT_YOU_USE "${INCLUDE_WHAT_YOU_USE}")
endif()


option(BUILD_DOCS "build the documentation" OFF)
if(BUILD_DOCS)
  find_package(Doxygen REQUIRED dot)
  set(DOXYGEN_MAN_LINKS YES)
  set(DOXYGEN_GENERATE_MAN YES)
  set(DOXYGEN_GENERATE_HTML YES)
  set(DOXYGEN_EXTRACT_LOCAL_METHODS YES)
  set(DOXYGEN_EXTRACT_STATIC YES)
  set(DOXYGEN_MACRO_EXPANSION YES)
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
  doxygen_add_docs(
    docs
    ${PROJECT_SOURCE_DIR}/README.md
    ${PROJECT_SOURCE_DIR}/include
    COMMENT "Generate Documenation"
  )
endif()
