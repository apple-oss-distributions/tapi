add_tapi_test_library(SubBar
  FRAMEWORK 
  SOURCE Reexports/Bar.m
  ARCHITECTURES x86_64 arm64
)
target_link_libraries(SubBar
  "-framework Foundation"
)

add_tapi_test_library(Umbrella4
  FRAMEWORK
  PUBLIC_HEADERS
  Headers/Umbrella4.h
  Headers/Bar.h
  SOURCE Umbrella4.m
  ARCHITECTURES x86_64 arm64
)

target_link_libraries(Umbrella4
  PRIVATE
  -F${TAPI_BINARY_DIR}/Inputs/System/Library/Frameworks
  -Wl,-reexport_framework,SubBar
  -L${TAPI_BINARY_DIR}/Inputs/usr/lib
)
add_dependencies(Umbrella4 SubBar)
