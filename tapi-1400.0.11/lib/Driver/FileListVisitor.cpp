//===- tapi/Driver/FileListVisitor.cpp - File List Visitor -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines a visitor implementation for retaining headers found in JSON
///        File List.
///
//===----------------------------------------------------------------------===//

#include "FileListVisitor.h"

TAPI_NAMESPACE_INTERNAL_BEGIN

void FileListVisitor::visitHeaderFile(HeaderType type, StringRef path) {
  if (!fm.exists(path)) {
    diag.report(diag::err_no_such_header_file) << path << (unsigned)type;
    return;
  }

  if (type == HeaderType::Project) {
    headerFiles.emplace_back(path, type);
    return;
  }

  auto includeName = createIncludeHeaderName(path);
  headerFiles.emplace_back(path, type, /*relativePath*/ "",
                           includeName.hasValue() ? includeName.getValue()
                                                  : "");
}

TAPI_NAMESPACE_INTERNAL_END
