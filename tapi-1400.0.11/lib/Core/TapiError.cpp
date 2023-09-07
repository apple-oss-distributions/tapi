//===- lib/Core/TapiError.cpp - Tapi Error ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Implements TAPI Error.
///
//===----------------------------------------------------------------------===//

#include "tapi/Core/TapiError.h"

using namespace llvm;

TAPI_NAMESPACE_INTERNAL_BEGIN

char TapiError::ID = 0;

void TapiError::log(raw_ostream &os) const {
  switch (ec) {
  case TapiErrorCode::NoSuchArchitecture:
    os << "no such architecture\n";
    return;
  default:
    llvm_unreachable("unhandled TapiErrorCode");
  }
}

std::error_code TapiError::convertToErrorCode() const {
  llvm_unreachable("convertToErrorCode is not supported.");
}

TAPI_NAMESPACE_INTERNAL_END
