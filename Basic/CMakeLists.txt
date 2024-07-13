add_library(basic
  arrays.c
  bcode.c
  builtin.c
  def.c
  emit.c
  env.c
  lexer.c
  paren.c
  parse.c
  run.c
  source.c
  token.c
)
if(UNIT_TESTS)
target_compile_definitions(basic PRIVATE UNIT_TEST)
endif()
set_target_properties(basic PROPERTIES C_STANDARD 11)
