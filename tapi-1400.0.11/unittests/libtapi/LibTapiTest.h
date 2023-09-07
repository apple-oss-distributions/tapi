//===-- LibTapiTest.h - Lib Tapi Test -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LIB_TAPI_TEST
#define LIB_TAPI_TEST

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"

#define ASSERT_NO_ERROR(x)                                                     \
  if (std::error_code ASSERT_NO_ERROR_ec = x) {                                \
    llvm::SmallString<128> messageStorage;                                     \
    llvm::raw_svector_ostream message(messageStorage);                         \
    message << #x ": did not return errc::success.\n"                          \
            << "error number: " << ASSERT_NO_ERROR_ec.value() << "\n"          \
            << "error message: " << ASSERT_NO_ERROR_ec.message() << "\n";      \
    GTEST_FATAL_FAILURE_(messageStorage.c_str());                              \
  } else {                                                                     \
  }

class LibTapiTest : public testing::Test {
private:
  llvm::SmallString<PATH_MAX> tmpFilePath;
  int tmpFileFD;

public:
  void SetUp() override {
    ASSERT_NO_ERROR(llvm::sys::fs::createTemporaryFile("Test", "tbd", tmpFileFD,
                                                       tmpFilePath));
  }

  void TearDown() override {
    ASSERT_NO_ERROR(llvm::sys::fs::remove(tmpFilePath.str()));
  }

  template <typename T> void writeTempFile(T str) {
    llvm::raw_fd_ostream file(tmpFileFD, /*shouldClose=*/true);
    file << str;
  }

  const char *getTempFilePath() { return tmpFilePath.c_str(); }
};

#endif // LIB_TAPI_TEST
