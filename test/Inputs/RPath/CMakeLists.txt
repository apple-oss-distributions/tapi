add_tapi_test_library(RPath
  FRAMEWORK
  PUBLIC_HEADERS RPath.h
  SOURCE rpath.m
  ARCHITECTURES arm64 x86_64
  INSTALL_NAME_DIR @rpath/Frameworks/
)
target_link_libraries(RPath 
  PRIVATE 
  objc
  "-rpath @loader_path/../../../SharedFrameworks/"
  "-rpath @loader_path/../../PrivateFrameworks/"
)
