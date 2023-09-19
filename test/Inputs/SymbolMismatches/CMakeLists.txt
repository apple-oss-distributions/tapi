add_tapi_test_library(Mismatch
  FRAMEWORK
  PUBLIC_HEADERS Mismatch.h
  PRIVATE_HEADERS Mismatch_Private.h
  SOURCE mismatch.m
  ARCHITECTURES arm64 x86_64
)

target_link_libraries(Mismatch
  PRIVATE
  objc
  -Wl,-iosmac_version_min,16.0
)

add_tapi_test_library(SymbolFlags
  FRAMEWORK
  PUBLIC_HEADERS Flags.h
  SOURCE flags.cpp
  ARCHITECTURES arm64 
)

add_tapi_test_library(MismatchCpp
  FRAMEWORK
  PUBLIC_HEADERS CPP.h
  SOURCE mismatch.mm
  ARCHITECTURES arm64 
)

target_link_libraries(MismatchCpp
  PRIVATE
  -Wl,-exported_symbol,"__ZN3Bar4initEv"
  -Wl,-exported_symbol,"__Z3fooIjEiT_"
)

add_tapi_test_library(SimpleMismatch
  FRAMEWORK
  PUBLIC_HEADERS simple.h
  SOURCE simple.m
  ARCHITECTURES x86_64 arm64
)

target_compile_options(SimpleMismatch
  PRIVATE
  -g -O3
)

add_tapi_test_library(SingleArchError
  FRAMEWORK
  PUBLIC_HEADERS simple2.h
  SOURCE simple.m
  ARCHITECTURES x86_64 arm64
)
