//===- API2XPIConverter.h - API2XPI Converter -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief API2XPI Converter
///
//===----------------------------------------------------------------------===//

#include "tapi/Core/APIVisitor.h"
#include "tapi/Core/XPISet.h"
#include "tapi/Defines.h"

#ifndef TAPI_DRIVER_API2XPICONVERTER_H
#define TAPI_DRIVER_API2XPICONVERTER_H

TAPI_NAMESPACE_INTERNAL_BEGIN

class API2XPIConverter : public APIVisitor {
public:
  API2XPIConverter(XPISet *xpiSet, const llvm::Triple &triple)
      : xpiSet(xpiSet), target(triple) {}
  void visitGlobal(const GlobalRecord &) override;
  void visitObjCInterface(const ObjCInterfaceRecord &) override;
  void visitObjCCategory(const ObjCCategoryRecord &) override;

private:
  XPISet *xpiSet;
  const Target target;
};

TAPI_NAMESPACE_INTERNAL_END

#endif // TAPI_DRIVER_API2XPICONVERTER_H
