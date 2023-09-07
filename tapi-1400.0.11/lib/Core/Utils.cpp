//===- lib/Core/Utils.cpp - TAPI Utility Methods ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Misc utility methods.
///
//===----------------------------------------------------------------------===//

#include "tapi/Core/Utils.h"
#include "tapi/Core/FileManager.h"
#include "tapi/Core/Path.h"
#include "tapi/Defines.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Path.h"

using namespace llvm;

TAPI_NAMESPACE_INTERNAL_BEGIN

bool isPublicDylib(StringRef path) {
  // Remove the iOSSupport/DriverKit prefix to identify public locations inside
  // the iOSSupport/DriverKit directory.
  path.consume_front("/System/iOSSupport");
  path.consume_front("/System/DriverKit");
  // Also /Library/Apple prefix for ROSP.
  path.consume_front("/Library/Apple");

  // Everything in /usr/lib/swift (including sub-directories) is now considered
  // public.
  if (path.consume_front("/usr/lib/swift/"))
    return true;

  // Only libraries directly in /usr/lib are public. All other libraries in
  // sub-directories (such as /usr/lib/system) are considered private.
  if (path.consume_front("/usr/lib/")) {
    if (path.contains('/'))
      return false;
    return true;
  }

  // /System/Library/Frameworks/ is a public location
  if (path.consume_front("/System/Library/Frameworks/")) {
    StringRef name, rest;
    std::tie(name, rest) = path.split('.');

    // but only top level framework
    // /System/Library/Frameworks/Foo.framework/Foo ==> true
    // /System/Library/Frameworks/Foo.framework/Versions/A/Foo ==> true
    // /System/Library/Frameworks/Foo.framework/Resources/libBar.dylib ==> false
    // /System/Library/Frameworks/Foo.framework/Frameworks/Bar.framework/Bar
    // ==> false
    // /System/Library/Frameworks/Foo.framework/Frameworks/XFoo.framework/XFoo
    // ==> false
    if (rest.startswith("framework/") && (sys::path::filename(rest) == name))
      return true;

    return false;
  }

  return false;
}

bool isWithinPublicLocation(StringRef path) {
  path.consume_front("/System/iOSSupport");
  path.consume_front("/System/DriverKit");
  path.consume_front("/Library/Apple");

  if (path.startswith("/usr/local/") || 
      path.startswith("/System/Library/PrivateFrameworks/"))
    return false;

  if (path.consume_front("/System/Library/Frameworks/")) {
    // Exclude everything from PrivateHeaders.
    while (!path.empty()) {
      auto split = path.split('/');
      if (split.first == "PrivateHeaders")
        return false;
      path = split.second;
    }
  }

  return true;
}

bool isHeaderFile(StringRef path) {
  return StringSwitch<bool>(sys::path::extension(path))
      .Cases(".h", ".H", ".hh", ".hpp", ".hxx", true)
      .Default(false);
}

std::string findLibrary(StringRef installName, FileManager &fm,
                        ArrayRef<std::string> frameworkSearchPaths,
                        ArrayRef<std::string> librarySearchPaths,
                        ArrayRef<std::string> searchPaths) {
  auto filename = sys::path::filename(installName);
  bool isFramework = sys::path::parent_path(installName)
                         .endswith((filename + ".framework").str());

  if (isFramework) {
    for (const auto &path : frameworkSearchPaths) {
      SmallString<PATH_MAX> fullPath(path);
      sys::path::append(fullPath, filename + StringRef(".framework"), filename);

      SmallString<PATH_MAX> tbdPath = fullPath;
      TAPI_INTERNAL::replace_extension(tbdPath, ".tbd");
      if (fm.exists(tbdPath))
        return tbdPath.str().str();

      if (fm.exists(fullPath))
        return fullPath.str().str();
    }
  } else {
    // Copy ld64's behavior: If this is a .dylib inside a framework, do not
    // search -L paths.
    bool embeddedDylib = (sys::path::extension(installName) == ".dylib") &&
                         installName.contains(".framework/");
    if (!embeddedDylib) {
      for (const auto &path : librarySearchPaths) {
        SmallString<PATH_MAX> fullPath(path);
        sys::path::append(fullPath, filename);

        SmallString<PATH_MAX> tbdPath = fullPath;
        TAPI_INTERNAL::replace_extension(tbdPath, ".tbd");

        if (fm.exists(tbdPath))
          return tbdPath.str().str();

        if (fm.exists(fullPath))
          return fullPath.str().str();
      }
    }
  }

  for (const auto &path : searchPaths) {
    SmallString<PATH_MAX> fullPath(path);
    sys::path::append(fullPath, installName);

    SmallString<PATH_MAX> tbdPath = fullPath;
    TAPI_INTERNAL::replace_extension(tbdPath, ".tbd");

    if (fm.exists(tbdPath))
      return tbdPath.str().str();

    if (fm.exists(fullPath))
      return fullPath.str().str();
  }

  return std::string();
}

TAPI_NAMESPACE_INTERNAL_END
