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
