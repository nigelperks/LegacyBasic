add_library(monitor
  interactive.c
)
if(UNIT_TESTS)
target_compile_definitions(monitor PRIVATE UNIT_TEST)
endif()
set_target_properties(monitor PROPERTIES C_STANDARD 11)
