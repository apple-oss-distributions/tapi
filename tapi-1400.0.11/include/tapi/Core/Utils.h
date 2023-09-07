//===- tapi/Core/Utils.h - TAPI Utility Methods -----------------*- C++ -*-===//
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

#ifndef TAPI_CORE_UTILS_H
#define TAPI_CORE_UTILS_H

#include "tapi/Core/LLVM.h"
#include "tapi/Defines.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

TAPI_NAMESPACE_INTERNAL_BEGIN

class FileManager;

/// Return true of the path to the dylib is considered a public location.
bool isPublicDylib(StringRef path);
/// Return true of the path is within a location of public framework.
bool isWithinPublicLocation(StringRef path);
/// Return true of the path has extension of a header.
bool isHeaderFile(StringRef path);
/// Search the path to find the library specificed by installName.
std::string findLibrary(StringRef installName, FileManager &fm,
                        ArrayRef<std::string> frameworkSearchPaths,
                        ArrayRef<std::string> librarySearchPaths,
                        ArrayRef<std::string> searchPaths);

TAPI_NAMESPACE_INTERNAL_END

#endif // TAPI_CORE_UTILS_H
