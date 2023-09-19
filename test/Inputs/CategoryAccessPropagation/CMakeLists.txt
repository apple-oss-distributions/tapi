add_tapi_test_library(CategoryAccessPropagation
  FRAMEWORK

  PUBLIC_HEADERS
  Public.h

  SOURCE Source.m
  CURRENT_VERSION 1.2.3.4.5
)

target_compile_options(CategoryAccessPropagation
  PRIVATE
  -w
  -F${TAPI_BINARY_DIR}/Inputs/System/Library/Frameworks
)

target_link_libraries(CategoryAccessPropagation
  PRIVATE
  "-framework Foundation"
  # this is necessary to prevent the linker from merging the category into the
  # class which would break the test
  -Wl,-no_objc_category_merging
)
