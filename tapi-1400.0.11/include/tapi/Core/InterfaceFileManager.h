//===- IntefaceFileManager.h - TAPI Interface File Manager ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Interface File Manager
///
//===----------------------------------------------------------------------===//

#ifndef TAPI_CORE_INTERFACE_FILE_MANAGER_H
#define TAPI_CORE_INTERFACE_FILE_MANAGER_H

#include "tapi/Core/InterfaceFile.h"
#include "tapi/Core/Registry.h"
#include "tapi/Defines.h"
#include <map>

TAPI_NAMESPACE_INTERNAL_BEGIN

class FileManager;

class InterfaceFileManager {
public:
  InterfaceFileManager(FileManager &fm);
  Expected<InterfaceFile *> readFile(const std::string &path);
  Error writeFile(const std::string &path, const InterfaceFile *file,
                  VersionedFileType fileType) const;

private:
  FileManager &_fm;
  Registry _registry;
  std::map<std::string, std::unique_ptr<InterfaceFile>> _libraries;
};

TAPI_NAMESPACE_INTERNAL_END

#endif // TAPI_CORE_INTERFACE_FILE_MANAGER_H
