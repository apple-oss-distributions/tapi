//===- lib/Frontend/FrontendContext.cpp - TAPI Frontend Context -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Implements the TAPI Frontend Context
///
//===----------------------------------------------------------------------===//

#include "clang/Lex/Preprocessor.h"
#include "tapi/Frontend/FrontendContext.h"

TAPI_NAMESPACE_INTERNAL_BEGIN

FrontendContext::FrontendContext(
    const llvm::Triple &triple, StringRef workingDirectory,
    IntrusiveRefCntPtr<FileSystemStatCacheFactory> cacheFactory,
    IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs)
    : target(triple), api(triple) {
  fileManager = new FileManager(
      clang::FileSystemOptions{workingDirectory.str()}, cacheFactory, vfs);
}

llvm::Optional<HeaderType>
FrontendContext::findAndRecordFile(const FileEntry *file) {
  if (!file)
    return llvm::None;

  auto it = knownFiles.find(file);
  if (it != knownFiles.end())
    return it->second;

  // Check if file was previously found, but not one tapi is interested in.
  auto unused = unusedFiles.find(file);
  if (unused != unusedFiles.end())
    return llvm::None;

  // If file was not found, search by how the header was
  // included. This is primarily to resolve headers found via headermaps, as
  // they remap locations.
  auto fileInfo = pp->getHeaderSearchInfo().getExistingFileInfo(file);
  if (!fileInfo || !fileInfo->IsValid)
    return llvm::None;

  StringRef fileName = file->getName();
  std::string includeName =
      fileInfo->Framework.empty()
          ? fileName.str()
          : (fileInfo->Framework + "/" + llvm::sys::path::filename(fileName))
                .str();

  auto backup = knownIncludes.find(includeName);
  if (backup == knownIncludes.end()) {
    // Record that the file was found to avoid future string searches for the
    // same file.
    unusedFiles.insert(file);
    return llvm::None;
  }

  knownFiles[file] = backup->second;
  return backup->second;
}

TAPI_NAMESPACE_INTERNAL_END
