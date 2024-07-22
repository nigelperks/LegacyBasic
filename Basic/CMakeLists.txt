add_library(basic
  arrays.c
  bcode.c
  builtin.c
  def.c
  emit.c
  init.c
  lexer.c
  linemap.c
  parse.c
  run.c
  source.c
  symbol.c
  token.c
)
if(UNIT_TESTS)
target_compile_definitions(basic PRIVATE UNIT_TEST)
endif()
set_target_properties(basic PROPERTIES C_STANDARD 11)
