add_tapi_test_library(Hidden
  FRAMEWORK
  PUBLIC_HEADERS Hidden.h

  SOURCE Hidden.m
)
target_compile_options(Hidden
  PRIVATE
  "-Wno-private-extern"
)
