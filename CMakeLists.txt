cmake_minimum_required(VERSION 3.22.1)
project(LegacyBasic DESCRIPTION "Legacy Basic" VERSION 3.2.0)
set(LegacyBasic_COPYRIGHT "Copyright (c) 2022-24 Nigel Perks")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}/Release)
include_directories("${PROJECT_SOURCE_DIR}/Shared"
                    "${PROJECT_SOURCE_DIR}/Basic"
                    "${PROJECT_SOURCE_DIR}/Monitor")
if(MSVC)
  add_compile_options(
    $<$<CONFIG:>:/MT>
    $<$<CONFIG:Debug>:/MTd>
    $<$<CONFIG:Release>:/MT>
  )
endif()
set(UNIT_TESTS ON)
add_subdirectory(Shared)
add_subdirectory(Basic)
add_subdirectory(Monitor)
add_subdirectory(System)

file(COPY_FILE "${PROJECT_SOURCE_DIR}/README.md" "${PROJECT_BINARY_DIR}/README.md")
file(COPY_FILE "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_BINARY_DIR}/LICENSE")
file(COPY_FILE "${PROJECT_SOURCE_DIR}/doc/LegacyBasic.rst" "${PROJECT_BINARY_DIR}/LegacyBasic.rst")

install(TARGETS LegacyBasic DESTINATION bin COMPONENT interpreter)
if(UNIX OR LINUX)
set(LBASIC_DOCDIR "share/legacy-basic")
else()
set(LBASIC_DOCDIR "doc")
endif()
install(FILES README.md LICENSE doc/LegacyBasic.rst
        DESTINATION ${LBASIC_DOCDIR} COMPONENT documentation)

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_NAME "LegacyBasic")
set(CPACK_PACKAGE_VERSION_MAJOR "3")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VENDOR "Nigel Perks")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "LegacyBasic")
set(CPACK_PACKAGE_EXECUTABLES "LegacyBasic" "LegacyBasic")
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_CREATE_DESKTOP_LINKS LegacyBasic)
include(CPack)
cpack_add_component(interpreter REQUIRED
  DISPLAY_NAME "LegacyBasic interpreter"
  DESCRIPTION "Interpreter for 1970s/80s BASIC games"
)
cpack_add_component(documentation
  DISPLAY_NAME "Documentation"
  DESCRIPTION "License and how to use"
)

if(NOT LBASIC_NOTEST)
  include(FindPython3)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  enable_testing()
  add_test(NAME UnitTests COMMAND LegacyBasic "--unit-tests")
  get_target_property(LBASIC_OUTPUT_NAME LegacyBasic OUTPUT_NAME)
  add_test(NAME RegressionTests
           COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/test.py"
                   "--tests=${PROJECT_SOURCE_DIR}/tests"
          )
endif()
