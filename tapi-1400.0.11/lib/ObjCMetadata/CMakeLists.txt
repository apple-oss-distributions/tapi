add_llvm_component_library(LLVMObjCMetadata
  ObjCMetadata.cpp
  ObjCMachOBinary.cpp

  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
  Analysis
  Core
  Object
  Support
  )
