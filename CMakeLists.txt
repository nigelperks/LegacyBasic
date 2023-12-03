cmake_minimum_required(VERSION 3.22.1)
project(LegacyBasic DESCRIPTION "Legacy Basic" VERSION 3.1.1)
set(LegacyBasic_COPYRIGHT "Copyright (c) 2022-3 Nigel Perks")
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")
include_directories("${PROJECT_BINARY_DIR}")
if(MSVC)
  add_compile_options(
    $<$<CONFIG:>:/MT>
    $<$<CONFIG:Debug>:/MTd>
    $<$<CONFIG:Release>:/MT>
  )
endif()
add_executable(LegacyBasic
  arrays.c
  bcode.c
  builtin.c
  CuTest.c
  def.c
  emit.c
  env.c
  interactive.c
  interrupt.c
  keyword.c
  lbasic.c
  lexer.c
  os.c
  paren.c
  parse.c
  run.c
  source.c
  stringlist.c
  stringuniq.c
  token.c
  utils.c
)
set_target_properties(LegacyBasic PROPERTIES C_STANDARD 11)
if(UNIX OR LINUX)
set(LBASIC_OUTPUT_NAME "legacy-basic")
target_link_libraries(LegacyBasic PRIVATE m)
else()
set(LBASIC_OUTPUT_NAME "LegacyBasic")
endif()
set_target_properties(LegacyBasic PROPERTIES OUTPUT_NAME ${LBASIC_OUTPUT_NAME})
target_compile_definitions(LegacyBasic PRIVATE UNIT_TEST)

file(COPY_FILE "${PROJECT_SOURCE_DIR}/README.md" "${PROJECT_BINARY_DIR}/README.md")
file(COPY_FILE "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_BINARY_DIR}/LICENSE")

install(TARGETS LegacyBasic DESTINATION bin COMPONENT interpreter)
if(UNIX OR LINUX)
set(LBASIC_DOCDIR "share/legacy-basic")
else()
set(LBASIC_DOCDIR "doc")
endif()
install(FILES README.md LICENSE DESTINATION ${LBASIC_DOCDIR} COMPONENT documentation)

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_NAME "LegacyBasic")
set(CPACK_PACKAGE_VERSION_MAJOR "${LegacyBasic_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${LegacyBasic_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${LegacyBasic_VERSION_PATCH}")
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
  add_test(NAME RegressionTests
           COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/test.py"
                   "--tests=${PROJECT_SOURCE_DIR}/tests"
                   "--bin=${LBASIC_OUTPUT_NAME}"
          )
endif()
