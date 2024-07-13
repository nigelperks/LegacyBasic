add_executable(LegacyBasic
  lbasic.c
  options.c
)
if(UNIT_TESTS)
target_compile_definitions(LegacyBasic PRIVATE UNIT_TEST)
endif()
target_link_libraries(LegacyBasic monitor basic shared)
set_target_properties(LegacyBasic PROPERTIES C_STANDARD 11)
