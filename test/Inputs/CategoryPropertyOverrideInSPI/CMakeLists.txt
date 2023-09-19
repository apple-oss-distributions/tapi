add_tapi_test_library(CategoryPropertyOverrideInSPI
  FRAMEWORK

  PUBLIC_HEADERS
  Public.h

  PRIVATE_HEADERS
  Private.h

  SOURCE Source.m
  CURRENT_VERSION 1.2.3.4.5
)

target_compile_options(CategoryPropertyOverrideInSPI
  PRIVATE
  -w
  -F${TAPI_BINARY_DIR}/Inputs/System/Library/Frameworks
)

target_link_libraries(CategoryPropertyOverrideInSPI
  PRIVATE
  "-framework Foundation"
)
