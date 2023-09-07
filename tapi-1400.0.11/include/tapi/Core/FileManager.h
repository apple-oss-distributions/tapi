//===- tapi/Core/FileManager.h - File Manager --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Extends the clang file manager.
///
//===----------------------------------------------------------------------===//

#ifndef TAPI_CORE_FILE_MANAGER_H
#define TAPI_CORE_FILE_MANAGER_H

#include "tapi/Core/LLVM.h"
#include "tapi/Defines.h"
#include "clang/Basic/FileManager.h"

TAPI_NAMESPACE_INTERNAL_BEGIN

class FileSystemStatCacheFactory
    : public llvm::RefCountedBase<FileSystemStatCacheFactory> {
public:
  virtual ~FileSystemStatCacheFactory() {}
  virtual clang::FileSystemStatCache *create() = 0;
};

template <typename T>
llvm::IntrusiveRefCntPtr<FileSystemStatCacheFactory>
newFileSystemStatCacheFactory() {
  class SimpleFileSystemStatCacheFactory : public FileSystemStatCacheFactory {
  public:
    clang::FileSystemStatCache *create() override { return new T; }
  };

  return llvm::IntrusiveRefCntPtr<FileSystemStatCacheFactory>(
      new SimpleFileSystemStatCacheFactory);
}

/// \brief Basically the clang FileManager with additonal convenience methods
///        and a recording stat cache.
class FileManager final : public clang::FileManager {
public:
  FileManager(const clang::FileSystemOptions &fileSystemOpts,
              llvm::IntrusiveRefCntPtr<FileSystemStatCacheFactory>
                  cacheFactory = nullptr,
              llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> fs = nullptr);

  /// \brief Check if a particular path exists.
  bool exists(StringRef path);

  /// \brief Check if a particular path is a directory.
  bool isDirectory(StringRef path, bool CacheFailure = true) {
    return (bool)getDirectory(path, CacheFailure);
  }

  /// \brief Check if a particular path is a symlink using directory_iterator.
  bool isSymlink(StringRef path);

  /// \brief Create and add stat cache.
  void installStatRecorder();

private:
  bool initWithVFS = false;
  llvm::IntrusiveRefCntPtr<FileSystemStatCacheFactory> cacheFactory;
};

TAPI_NAMESPACE_INTERNAL_END

#endif // TAPI_CORE_FILE_MANAGER_H
