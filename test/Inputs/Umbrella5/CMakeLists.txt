add_tapi_test_library(TwinMac
  FRAMEWORK

  SOURCE Bar.m
  OUTPUT_NAME Twin
)

add_tapi_test_library(TwinMacCatalyst
  FRAMEWORK

  SOURCE Bar.m
  INSTALL_NAME_DIR /System/iOSSupport/System/Library/Frameworks
  OUTPUT_DIRECTORY /System/iOSSupport/System/Library/Frameworks
  OUTPUT_NAME Twin
)

target_compile_options(TwinMacCatalyst
  PRIVATE
  -target x86_64-apple-ios13.1-macabi
)
target_link_libraries(TwinMacCatalyst
  "-target x86_64-apple-ios13.1-macabi"
)

add_tapi_test_library(Umbrella5
  FRAMEWORK
  PUBLIC_HEADERS
  Headers/Bar.h
  
  SOURCE Umbrella.m
  INSTALL_NAME_DIR /System/iOSSupport/System/Library/Frameworks
  OUTPUT_DIRECTORY /System/iOSSupport/System/Library/Frameworks
)

target_link_libraries(Umbrella5
  PRIVATE
  "-target x86_64-apple-ios13.1-macabi"
  -F${TAPI_BINARY_DIR}/Inputs/System/iOSSupport/System/Library/Frameworks
  -Wl,-reexport_framework,Twin
)

target_compile_options(Umbrella5
  PRIVATE
  -target x86_64-apple-ios13.1-macabi
)


add_dependencies(Umbrella5 TwinMac)
add_dependencies(Umbrella5 TwinMacCatalyst)
