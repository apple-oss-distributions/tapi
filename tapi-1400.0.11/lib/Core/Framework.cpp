//===- lib/Core/Framework.cpp - Framework Context ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Implements the Framework context.
///
//===----------------------------------------------------------------------===//

#include "tapi/Core/Framework.h"
#include "tapi/Core/HeaderFile.h"
#include "llvm/ADT/None.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include <set>

using namespace llvm;

TAPI_NAMESPACE_INTERNAL_BEGIN

StringRef Framework::getName() const {
  StringRef path = _baseDirectory;
  // Returns the framework name extract from path.
  while (!path.empty()) {
    if (path.endswith(".framework"))
      return sys::path::filename(path);
    path = sys::path::parent_path(path);
  }

  // Otherwise, return the name of the baseDirectory.
  // First, remove all the trailing seperator.
  path = _baseDirectory;
  return sys::path::filename(path.rtrim("/"));
}

SwiftModule::SwiftModule(StringRef path) {
  auto filename = sys::path::filename(path);
  if (filename.consume_back(".swiftmodule"))
    name = filename.str();
  else if (auto n = filename.consume_back(".swiftinterface"))
    name = filename.str();
  else
    llvm_unreachable("unexpected file extension");
}

StringRef Framework::getAdditionalIncludePath() const {
  if (isSysRoot || !isDynamicLibrary)
    return StringRef();

  return _baseDirectory;
}

StringRef Framework::getAdditionalFrameworkPath() const {
  if (isSysRoot || isDynamicLibrary)
    return StringRef();

  auto parentPath = sys::path::parent_path(_baseDirectory);
  if (sys::path::filename(_baseDirectory).endswith(".framework"))
    return parentPath;

  // For macOS legacy style framework layout.
  if (parentPath.endswith("Versions"))
    return sys::path::parent_path(sys::path::parent_path(parentPath));

  return StringRef();
}

TAPI_NAMESPACE_INTERNAL_END
