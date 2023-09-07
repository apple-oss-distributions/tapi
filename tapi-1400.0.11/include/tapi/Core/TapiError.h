//===- tapi/Core/TapiError.h - TAPI Error -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Define TAPI specific error codes.
///
//===----------------------------------------------------------------------===//

#ifndef TAPI_CORE_TAPIERROR_H
#define TAPI_CORE_TAPIERROR_H

#include "tapi/Core/LLVM.h"
#include "tapi/Defines.h"
#include "llvm/Support/Error.h"

TAPI_NAMESPACE_INTERNAL_BEGIN

enum class TapiErrorCode {
  NoSuchArchitecture,
  EmptyResults,
  GenericFrontendError,
};

class TapiError : public llvm::ErrorInfo<TapiError> {
public:
  static char ID;
  TapiErrorCode ec;

  TapiError(TapiErrorCode ec) : ec(ec) {}

  void log(raw_ostream &os) const override;
  std::error_code convertToErrorCode() const override;
};

TAPI_NAMESPACE_INTERNAL_END

#endif // TAPI_CORE_TAPIERROR_H
