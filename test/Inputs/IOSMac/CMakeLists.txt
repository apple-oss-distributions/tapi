add_tapi_test_library(IOSMac
  FRAMEWORK
  PUBLIC_HEADERS IOSMac.h
  SOURCE IOSMac.m
  INSTALL_NAME_DIR /System/iOSSupport/System/Library/Frameworks
  OUTPUT_DIRECTORY /System/iOSSupport/System/Library/Frameworks
)
target_compile_options(IOSMac
  PRIVATE
  -target x86_64-apple-ios13.1-macabi
)
target_link_libraries(IOSMac
  PRIVATE
  objc
  "-target x86_64-apple-ios13.1-macabi"
)
