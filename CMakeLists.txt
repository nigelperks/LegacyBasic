cmake_minimum_required(VERSION 3.22.1)
project(LegacyBasic)
if(MSVC)
  add_compile_options(
    $<$<CONFIG:>:/MT>
    $<$<CONFIG:Debug>:/MTd>
    $<$<CONFIG:Release>:/MT>
  )
endif()
add_executable(LegacyBasic arrays.c bcode.c builtin.c CuTest.c def.c emit.c env.c interactive.c interrupt.c keyword.c lbasic.c lexer.c paren.c parse.c run.c source.c stringlist.c token.c utils.c)
set_property(TARGET LegacyBasic PROPERTY C_STANDARD 11)
if(NOT WIN32)
target_link_libraries(LegacyBasic PRIVATE m)
endif()
target_compile_definitions(LegacyBasic PRIVATE UNIT_TEST)
install(TARGETS LegacyBasic DESTINATION bin)
