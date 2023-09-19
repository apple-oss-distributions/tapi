add_tapi_test_library(CodeCoverage
  FRAMEWORK
  PUBLIC_HEADERS CodeCoverage.h
  SOURCE CodeCoverage.m
)
target_compile_options(CodeCoverage
  PRIVATE
  -fprofile-instr-generate
  -target x86_64-apple-macosx13
)
target_link_libraries(CodeCoverage
  PRIVATE
  -fprofile-instr-generate
)
