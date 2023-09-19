add_tapi_test_library(ObjCLib
  PUBLIC_HEADERS ObjCLib.h
  SOURCE ObjCLib.m
  ARCHITECTURES x86_64
)
target_link_libraries(ObjCLib PRIVATE objc)

add_tapi_test_library(ObjCClient
  SOURCE ObjCClient.m
  ARCHITECTURES x86_64
)
target_link_libraries(ObjCClient PRIVATE objc "-framework Foundation" ObjCLib)
