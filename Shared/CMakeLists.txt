add_library(shared
  hash.c
  interrupt.c
  os.c
  stringlist.c
  stringuniq.c
  utils.c
)
if(UNIT_TESTS)
target_sources(shared PRIVATE CuTest.c)
target_compile_definitions(shared PRIVATE UNIT_TEST)
endif()
set_target_properties(shared PROPERTIES C_STANDARD 11)
