//===- tapi/Frontend/DeclarationFragmentsBuildier.h -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the TAPI Declaration Fragments Builder
///
//===----------------------------------------------------------------------===//
#ifndef TAPI_FRONTEND_DECLARATION_FRAGMENTS_BUILDER_H
#define TAPI_FRONTEND_DECLARATION_FRAGMENTS_BUILDER_H

#include "tapi/Core/API.h"
#include "tapi/Defines.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclObjC.h"
#include "clang/Lex/MacroInfo.h"

using namespace clang;

TAPI_NAMESPACE_INTERNAL_BEGIN

class DeclarationFragmentsBuilder {
public:
  static DeclarationFragments getFragmentsForMacro(const Token &token,
                                                   const MacroDirective *);
  static DeclarationFragments getFragmentsForVar(const VarDecl *);
  static DeclarationFragments getFragmentsForFunction(const FunctionDecl *);
  static DeclarationFragments getFragmentsForEnum(const EnumDecl *);
  static DeclarationFragments
  getFragmentsForEnumConstant(const EnumConstantDecl *);
  static DeclarationFragments
  getFragmentsForObjCInterface(const ObjCInterfaceDecl *);
  static DeclarationFragments getFragmentsForField(const FieldDecl *);
  static DeclarationFragments getFragmentsForObjCMethod(const ObjCMethodDecl *);
  static DeclarationFragments
  getFragmentsForObjCProperty(const ObjCPropertyDecl *);
  static DeclarationFragments
  getFragmentsForObjCCategory(const ObjCCategoryDecl *);
  static DeclarationFragments
  getFragmentsForObjCProtocol(const ObjCProtocolDecl *);
  static DeclarationFragments getFragmentsForTypedef(const TypedefNameDecl *);
  static DeclarationFragments getFragmentsForStruct(const RecordDecl *);

  static DeclarationFragments getSubHeading(const NamedDecl *);
  static DeclarationFragments getSubHeading(const ObjCMethodDecl *);
  static FunctionSignature getFunctionSignature(const FunctionDecl *);
  static FunctionSignature getSignatureForObjCMethod(const ObjCMethodDecl *);

private:
  DeclarationFragmentsBuilder() = delete;

  static DeclarationFragments getFragmentsForType(const QualType, ASTContext &,
                                                  DeclarationFragments &);
  static DeclarationFragments getFragmentsForType(const Type *, ASTContext &,
                                                  DeclarationFragments &);
  static DeclarationFragments getFragmentsForNNS(const NestedNameSpecifier *,
                                                 ASTContext &,
                                                 DeclarationFragments &);
  static DeclarationFragments getFragmentsForQualifiers(const Qualifiers quals);

  static DeclarationFragments getFragmentsForParam(const ParmVarDecl *);
};

TAPI_NAMESPACE_INTERNAL_END

#endif // TAPI_FRONTEND_DECLARATION_FRAGMENTS_BUILDER_H
