//===- lib/Driver/ExtractAPIDriver.cpp - TAPI Driver ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Implements the driver for the tapi tool.
///
//===----------------------------------------------------------------------===//

#include "APINormalizer.h"
#include "FileListVisitor.h"
#include "tapi/Core/APIJSONSerializer.h"
#include "tapi/Core/APIPrinter.h"
#include "tapi/Core/ClangDiagnostics.h"
#include "tapi/Core/HeaderFile.h"
#include "tapi/Core/Path.h"
#include "tapi/Core/Utils.h"
#include "tapi/Defines.h"
#include "tapi/Diagnostics/Diagnostics.h"
#include "tapi/Driver/Driver.h"
#include "tapi/Driver/HeaderGlob.h"
#include "tapi/Driver/Options.h"
#include "tapi/Driver/Snapshot.h"
#include "tapi/Driver/StatRecorder.h"
#include "tapi/Frontend/Frontend.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <string>

using namespace llvm;
using namespace llvm::opt;
using namespace clang;

TAPI_NAMESPACE_INTERNAL_BEGIN

/// \brief Parses the headers and extracts APIs.
bool Driver::ExtractAPI::run(DiagnosticsEngine &diag, Options &opts) {
  auto &fm = opts.getFileManager();

  // Handle targets.
  if (opts.frontendOptions.targets.empty()) {
    diag.report(diag::err_no_target);
    return false;
  }

  // Set default language option.
  if (opts.frontendOptions.language == clang::Language::Unknown)
    opts.frontendOptions.language = clang::Language::ObjC;

  diag.setErrorLimit(opts.diagnosticsOptions.errorLimit);

  PathSeq frameworkSearchPaths;
  PathSeq systemFrameworkPaths =
      getAllPaths(opts.frontendOptions.systemFrameworkPaths);

  // Search user framework paths before searching system framework paths.
  for (auto &path : opts.frontendOptions.frameworkPaths)
    frameworkSearchPaths.emplace_back(path);
  for (auto &path : systemFrameworkPaths)
    frameworkSearchPaths.emplace_back(path);

  if (opts.driverOptions.inputs.empty()) {
    diag.report(clang::diag::err_drv_no_input_files);
    return false;
  }

  if (opts.driverOptions.outputPath.empty()) {
    diag.report(diag::err_no_output_file);
    return false;
  }

  auto file = fm.getFile(opts.driverOptions.inputs.front());
  if (!file) {
    diag.report(clang::diag::err_drv_no_such_file)
        << opts.driverOptions.inputs.front();
    return false;
  }

  FrontendJob job;
  job.workingDirectory = globalSnapshot->getWorkingDirectory().str();
  job.cacheFactory = newFileSystemStatCacheFactory<StatRecorder>();
  job.vfs = &fm.getVirtualFileSystem();
  job.language = opts.frontendOptions.language;
  job.language_std = opts.frontendOptions.language_std;
  job.overwriteRTTI = opts.frontendOptions.useRTTI;
  job.overwriteNoRTTI = opts.frontendOptions.useNoRTTI;
  job.isysroot = opts.frontendOptions.isysroot;
  job.macros = opts.frontendOptions.macros;
  job.systemFrameworkPaths = systemFrameworkPaths;
  job.systemIncludePaths = opts.frontendOptions.systemIncludePaths;
  job.quotedIncludePaths = opts.frontendOptions.quotedIncludePaths;
  job.frameworkPaths = opts.frontendOptions.frameworkPaths;
  job.includePaths = opts.frontendOptions.includePaths;
  job.clangExtraArgs = opts.frontendOptions.clangExtraArgs;
  job.enableModules = opts.frontendOptions.enableModules;
  job.moduleCachePath = opts.frontendOptions.moduleCachePath;
  job.validateSystemHeaders = opts.frontendOptions.validateSystemHeaders;
  job.clangResourcePath = opts.frontendOptions.clangResourcePath;
  job.useObjectiveCARC = opts.frontendOptions.useObjectiveCARC;
  job.useObjectiveCWeakARC = opts.frontendOptions.useObjectiveCWeakARC;
  job.verbose = opts.frontendOptions.verbose;
  job.clangExecutablePath = opts.driverOptions.clangExecutablePath;

  HeaderSeq headerFiles;

  auto bufferOr = fm.getBufferForFile(*file);
  if (auto ec = bufferOr.getError()) {
    diag.report(diag::err_cannot_read_file)
        << (*file)->getName() << ec.message();
    return false;
  }
  auto reader = FileListReader::get(std::move(bufferOr.get()));
  if (!reader) {
    diag.report(diag::err_cannot_read_file)
        << (*file)->getName() << toString(reader.takeError());
    return false;
  }

  FileListVisitor visitor(fm, diag, headerFiles);
  reader.get()->visit(visitor);
  if (diag.hasErrorOccurred())
    return false;

  job.headerFiles = headerFiles;

  std::map<HeaderType, std::vector<FrontendContext>> frontendResults;
  for (auto type :
       {HeaderType::Public, HeaderType::Private, HeaderType::Project}) {
    for (auto &target : opts.frontendOptions.targets) {
      job.target = target;
      job.type = type;
      auto contextOrError = runFrontend(job);
      if (auto err = contextOrError.takeError()) {
        if (canIgnoreFrontendError(err))
          continue;
        return false;
      }
      frontendResults[type].emplace_back(std::move(*contextOrError));
    }
  }

  if (opts.tapiOptions.printAfter == "frontend") {
    APIPrinter printer(errs());
    errs() << "public:"
           << "\n";
    for (auto &result : frontendResults[HeaderType::Public]) {
      errs() << "triple:" << result.target.str() << "\n";
      result.visit(printer);
      errs() << "\n";
    }

    errs() << "private:"
           << "\n";
    for (auto &result : frontendResults[HeaderType::Private]) {
      errs() << "triple:" << result.target.str() << "\n";
      result.visit(printer);
      errs() << "\n";
    }

    errs() << "project:"
           << "\n";
    for (auto &result : frontendResults[HeaderType::Project]) {
      errs() << "triple:" << result.target.str() << "\n";
      result.visit(printer);
      errs() << "\n";
    }
  }

  SmallString<PATH_MAX> outputDir(opts.driverOptions.outputPath);
  sys::path::remove_filename(outputDir);
  std::error_code errorCode = sys::fs::create_directories(outputDir);
  if (errorCode) {
    diag.report(diag::err_cannot_create_directory)
        << outputDir << errorCode.message();
    return false;
  }

  raw_fd_ostream fs(opts.driverOptions.outputPath, errorCode);
  if (errorCode) {
    diag.report(diag::err_cannot_open_file)
        << opts.driverOptions.outputPath << errorCode.message();
    return false;
  }

  // Normalize the file paths.
  APINormalizer normalizer;
  for (auto type :
       {HeaderType::Public, HeaderType::Private, HeaderType::Project}) {
    for (auto &result : frontendResults[type]) {
      result.api.visit(normalizer);
    }
  }

  // TODO: determine how should we invoke tapi extractapi for
  // public/private/project headers, and how to organize the outputs.
  // * One invocation to parse all types of headers in the filelist?
  // * Three separate invocations?
  // * Specific flags for each invocation?
  // * Necessary to output results in separate sections given that
  //   there's a access level field for each API record?
  json::Object root;
  json::Array publicList;
  for (const FrontendContext &frontendContext :
       frontendResults[HeaderType::Public]) {
    APIJSONSerializer serializer(frontendContext.api);
    publicList.emplace_back(serializer.getJSONObject());
  }
  root["Public"] = std::move(publicList);

  json::Array privateList;
  for (const FrontendContext &frontendContext :
       frontendResults[HeaderType::Private]) {
    APIJSONSerializer serializer(frontendContext.api);
    privateList.emplace_back(serializer.getJSONObject());
  }
  root["Private"] = std::move(privateList);

  json::Array projectList;
  for (const FrontendContext &frontendContext :
       frontendResults[HeaderType::Project]) {
    APIJSONSerializer serializer(frontendContext.api);
    projectList.emplace_back(serializer.getJSONObject());
  }
  root["Project"] = std::move(projectList);

  // FIXME: don't write out whitespaces eventually.
  // Maybe add a pretty-print option
  fs << formatv("{0:2}", json::Value(std::move(root))) << "\n";

  return true;
}

TAPI_NAMESPACE_INTERNAL_END
