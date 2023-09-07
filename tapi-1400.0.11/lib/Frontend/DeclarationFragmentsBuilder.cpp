//===- tapi/Frontend/DeclarationFragmentsBuildier.cpp -----------*- C++ -*-===//
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

#include "DeclarationFragmentsBuilder.h"
#include "APIVisitor.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclObjCCommon.h"
#include "clang/Basic/Specifiers.h"
#include "clang/Index/USRGeneration.h"

TAPI_NAMESPACE_INTERNAL_BEGIN

// nns stores C++ nested name specifiers, which are prefixes to qualified names.
// For example `tapi::internal::diag::` in `tapi::internal::diag::err`.
// Build declaration fragments for nns recursively so that we have the USR for
// every part in a qualified name, and also leaves the actual underlying type
// cleaner for its own fragment.
DeclarationFragments
DeclarationFragmentsBuilder::getFragmentsForNNS(const NestedNameSpecifier *nns,
                                                ASTContext &context,
                                                DeclarationFragments &after) {
  DeclarationFragments fragments;
  if (nns->getPrefix())
    fragments.append(getFragmentsForNNS(nns->getPrefix(), context, after));

  switch (nns->getKind()) {
  case NestedNameSpecifier::Identifier:
    fragments.append(nns->getAsIdentifier()->getName(),
                     DeclarationFragments::Identifier);
    break;

  case NestedNameSpecifier::Namespace: {
    const NamespaceDecl *ns = nns->getAsNamespace();
    if (ns->isAnonymousNamespace())
      return fragments;
    SmallString<128> nsUSR;
    clang::index::generateUSRForDecl(ns, nsUSR);
    fragments.append(ns->getName(), DeclarationFragments::Identifier, nsUSR);
    break;
  }

  case NestedNameSpecifier::NamespaceAlias: {
    const NamespaceAliasDecl *nsAlias = nns->getAsNamespaceAlias();
    SmallString<128> nsAliasUSR;
    clang::index::generateUSRForDecl(nsAlias, nsAliasUSR);
    fragments.append(nsAlias->getName(), DeclarationFragments::Identifier,
                     nsAliasUSR);
    break;
  }

  case NestedNameSpecifier::Global:
    // The global specifier `::` at the beginning. No stored value.
    break;

  case NestedNameSpecifier::Super:
    // Microsoft's `__super` specifier.
    // FIXME: Do we care about this? Also a CXXRecordDecl* is stored with this
    // specifier, should we attach a USR for that?
    fragments.append("__super", DeclarationFragments::Keyword);
    break;

  case NestedNameSpecifier::TypeSpecWithTemplate:
    // A type prefixed by the `template` keyword.
    fragments.append("template", DeclarationFragments::Keyword);
    fragments.appendSpace();
    // Fallthrough after adding the keyword to handle the actual type.
    LLVM_FALLTHROUGH;

  case NestedNameSpecifier::TypeSpec: {
    const Type *type = nns->getAsType();
    // FIXME: Handle C++ template specialization type
    fragments.append(getFragmentsForType(type, context, after));
    break;
  }
  }

  // Add the separator text `::` for this segment.
  return fragments.append("::", DeclarationFragments::Text);
}

// Recursively build the declaration fragments for an underlying `Type` with
// qualifiers removed.
DeclarationFragments DeclarationFragmentsBuilder::getFragmentsForType(
    const Type *type, ASTContext &context, DeclarationFragments &after) {
  assert(type && "invalid type");

  DeclarationFragments fragments;

  // Declaration fragments of a pointer type is the declaration fragments of
  // the pointee type followed by a `*`, except for Objective-C `id` and `Class`
  // pointers, where we do not spell out the `*`.
  if (type->isPointerType() ||
      (type->isObjCObjectPointerType() &&
       !type->getAs<ObjCObjectPointerType>()->isObjCIdOrClassType())) {
    return fragments
        .append(getFragmentsForType(type->getPointeeType(), context, after))
        .append(" *", DeclarationFragments::Text);
  }

  // Declaration fragments of a lvalue reference type is the declaration
  // fragments of the underlying type followed by a `&`.
  if (const LValueReferenceType *lRefType = dyn_cast<LValueReferenceType>(type))
    return fragments
        .append(getFragmentsForType(lRefType->getPointeeTypeAsWritten(),
                                    context, after))
        .append(" &", DeclarationFragments::Text);

  // Declaration fragments of a rvalue reference type is the declaration
  // fragments of the underlying type followed by a `&&`.
  if (const RValueReferenceType *rRefType = dyn_cast<RValueReferenceType>(type))
    return fragments
        .append(getFragmentsForType(rRefType->getPointeeTypeAsWritten(),
                                    context, after))
        .append(" &&", DeclarationFragments::Text);

  // Declaration fragments of an array-typed variable have two parts:
  // 1. the element type of the array that appears before the variable name;
  // 2. array brackets `[(0-9)?]` that appear after the variable name.
  if (const ArrayType *arrayType = type->getAsArrayTypeUnsafe()) {
    // Build the "after" part first because the inner element type might also
    // be an array-type. For example `int matrix[3][4]` which has a type of
    // "(array 3 of (array 4 of ints))."
    // Push the array size part first to make sure they are in the right order.
    after.append("[", DeclarationFragments::Text);

    switch (arrayType->getSizeModifier()) {
    case ArrayType::Normal:
      break;
    case ArrayType::Static:
      fragments.append("static", DeclarationFragments::Keyword);
      break;
    case ArrayType::Star:
      fragments.append("*", DeclarationFragments::Text);
      break;
    }

    if (const ConstantArrayType *constArrayType =
            dyn_cast<ConstantArrayType>(arrayType)) {
      // FIXME: right now this would evaluate any expressions/macros written in
      // the original source to concrete values. For example
      // `int nums[MAX]` -> `int nums[100]`
      // `char *str[5 + 1]` -> `char *str[6]`
      SmallString<128> size;
      constArrayType->getSize().toStringUnsigned(size);
      after.append(size, DeclarationFragments::NumericLiteral);
    }

    after.append("]", DeclarationFragments::Text);

    return fragments.append(
        getFragmentsForType(arrayType->getElementType(), context, after));
  }

  // An ElaboratedType is a sugar for types that are referred to using an
  // elaborated keyword, e.g., `struct S`, `enum E`, or (in C++) via a
  // qualified name, e.g., `N::M::type`, or both.
  if (const ElaboratedType *elabType = dyn_cast<ElaboratedType>(type)) {
    ElaboratedTypeKeyword keyword = elabType->getKeyword();
    if (keyword != ETK_None) {
      fragments
          .append(ElaboratedType::getKeywordName(keyword),
                  DeclarationFragments::Keyword)
          .appendSpace();
    }

    if (const NestedNameSpecifier *nns = elabType->getQualifier())
      fragments.append(getFragmentsForNNS(nns, context, after));

    // After handling the elaborated keyword or qualified name, build
    // declaration fragments for the desugared underlying type.
    return fragments.append(
        getFragmentsForType(elabType->desugar(), context, after));
  }

  // FIXME: Function pointer/block type is not handled.

  // Everything we care about has been handled now, reduce to the canonical
  // unqualified base type.
  QualType base = type->getCanonicalTypeUnqualified();

  // Render the Objective-C `id`/`instancetype` as keywords.
  if (type->isObjCIdType())
    return fragments.append(base.getAsString(), DeclarationFragments::Keyword);

  // If the type is a typedefed type, get the underlying TypedefNameDecl for a
  // direct reference to the typedef instead of the wrapped type.
  if (const TypedefType *typedefType = dyn_cast<TypedefType>(type)) {
    const TypedefNameDecl *decl = typedefType->getDecl();
    const QualType innerTy = decl->getUnderlyingType();
    if (innerTy->isTypedefNameType()) {
      // if this is a typedef to another typedef, use the typedef's USR for the
      // fragment - this will actually be in the output, unlike a typedef to an
      // anonymous decl
      SmallString<128> declUSR;
      clang::index::generateUSRForDecl(decl, declUSR);
      return fragments.append(decl->getName(),
                              DeclarationFragments::TypeIdentifier, declUSR);
    }

    std::string usr =
        clang::APIVisitor::getUnderlyingTypeInfo(innerTy, context).usr.str();
    return fragments.append(decl->getName(),
                            DeclarationFragments::TypeIdentifier, usr);
  }

  // If the base type is a TagType (struct/interface/union/class/enum), let's
  // get the underlying Decl for better names and USRs.
  if (const TagType *tagType = dyn_cast<TagType>(base)) {
    const TagDecl *decl = tagType->getDecl();
    // Anonymous decl, skip this fragment.
    if (decl->getName().empty())
      return fragments;
    SmallString<128> tagUSR;
    clang::index::generateUSRForDecl(decl, tagUSR);
    return fragments.append(decl->getName(), DeclarationFragments::TypeIdentifier,
                            tagUSR);
  }

  // If the base type is an ObjCInterfaceType, get the underlying Decl for
  // better USRs.
  if (const ObjCInterfaceType *objCIType = dyn_cast<ObjCInterfaceType>(base)) {
    const ObjCInterfaceDecl *decl = objCIType->getDecl();
    SmallString<128> interfaceUSR;
    clang::index::generateUSRForDecl(decl, interfaceUSR);
    return fragments.append(decl->getName(), DeclarationFragments::TypeIdentifier,
                            interfaceUSR);
  }

  // Default fragment builder for other kinds of types (BuiltinType etc.)
  SmallString<128> baseUSR;
  clang::index::generateUSRForType(base, context, baseUSR);
  fragments.append(base.getAsString(), DeclarationFragments::TypeIdentifier,
                   baseUSR);

  return fragments;
}

DeclarationFragments
DeclarationFragmentsBuilder::getFragmentsForQualifiers(const Qualifiers quals) {
  DeclarationFragments fragments;
  if (quals.hasConst())
    fragments.append("const", DeclarationFragments::Keyword);
  if (quals.hasVolatile())
    fragments.append("volatile", DeclarationFragments::Keyword);
  if (quals.hasRestrict())
    fragments.append("restrict", DeclarationFragments::Keyword);
  // FIXME: Handle non-CVR qualifiers that we care about

  return fragments;
}

DeclarationFragments DeclarationFragmentsBuilder::getFragmentsForType(
    const QualType type, ASTContext &context, DeclarationFragments &after) {
  assert(!type.isNull() && "invalid type");

  if (const ParenType *parenType = dyn_cast<ParenType>(type)) {
    after.append(")", DeclarationFragments::Text);
    return getFragmentsForType(parenType->getInnerType(), context, after)
        .append("(", DeclarationFragments::Text);
  }

  const SplitQualType splitType = type.split();
  DeclarationFragments fragmentsForQuals =
                           getFragmentsForQualifiers(splitType.Quals),
                       fragmentsForType =
                           getFragmentsForType(splitType.Ty, context, after);
  if (fragmentsForQuals.getFragments().empty())
    return fragmentsForType;

  // Use east qualifier for pointer types
  // For example:
  // ```
  // int *   const
  // ^----   ^----
  //  type    qualifier
  // ^-----------------
  //  const pointer to int
  // ```
  // should not be reconstructed as
  // ```
  // const       int       *
  // ^----       ^--
  //  qualifier   type
  // ^----------------     ^
  //  pointer to const int
  // ```
  if (splitType.Ty->isAnyPointerType())
    return getFragmentsForType(splitType.Ty, context, after)
        .appendSpace()
        .append(getFragmentsForQualifiers(splitType.Quals));

  return getFragmentsForQualifiers(splitType.Quals)
      .appendSpace()
      .append(getFragmentsForType(splitType.Ty, context, after));
}

DeclarationFragments
DeclarationFragmentsBuilder::getFragmentsForVar(const VarDecl *var) {
  DeclarationFragments fragments;
  StorageClass sc = var->getStorageClass();
  if (sc != SC_None)
    fragments
        .append(VarDecl::getStorageClassSpecifierString(sc),
                DeclarationFragments::Keyword)
        .appendSpace();
  QualType type =
      var->getTypeSourceInfo()
          ? var->getTypeSourceInfo()->getType()
          : var->getASTContext().getUnqualifiedObjCPointerType(var->getType());

  // Capture potential fragments that needs to be placed after the variable name
  // ```
  // int nums[5];
  // char (*ptr_to_array)[6];
  // ```
  DeclarationFragments after;
  return fragments
      .append(getFragmentsForType(type, var->getASTContext(), after))
      .appendSpace()
      .append(var->getName(), DeclarationFragments::Identifier)
      .append(std::move(after));
  // FIXME: Do we need to include attributes in declaration fragments?
}

DeclarationFragments
DeclarationFragmentsBuilder::getFragmentsForMacro(const Token &token,
                                                  const MacroDirective *md) {
  DeclarationFragments fragments;
  fragments.append("#define", DeclarationFragments::Keyword).appendSpace();
  fragments.append(token.getIdentifierInfo()->getName(),
                   DeclarationFragments::Identifier);

  auto mi = md->getMacroInfo();

  if (mi->isFunctionLike()) {
    fragments.append("(", DeclarationFragments::Text);
    unsigned numParameters = mi->getNumParams();
    if (mi->isC99Varargs())
      --numParameters;
    for (unsigned i = 0; i < numParameters; ++i) {
      if (i)
        fragments.append(", ", DeclarationFragments::Text);
      fragments.append(mi->params()[i]->getName(),
                       DeclarationFragments::InternalParameter);
    }
    if (mi->isVariadic()) {
      if (numParameters && mi->isC99Varargs())
        fragments.append(", ", DeclarationFragments::Text);
      fragments.append("...", DeclarationFragments::Text);
    }
    fragments.append(")", DeclarationFragments::Text);
  }

  return fragments;
}

DeclarationFragments
DeclarationFragmentsBuilder::getFragmentsForFunction(const FunctionDecl *func) {
  DeclarationFragments fragments;
  // FIXME: Handle template specialization
  switch (func->getStorageClass()) {
  case SC_None:
  case SC_PrivateExtern:
    break;
  case SC_Extern:
    fragments.append("extern", DeclarationFragments::Keyword).appendSpace();
    break;
  case SC_Static:
    fragments.append("static", DeclarationFragments::Keyword).appendSpace();
    break;
  case SC_Auto:
  case SC_Register:
    llvm_unreachable("invalid for functions");
  }
  // FIXME: Handle C++ function specifiers: constexpr, consteval, explicit, etc.

  // FIXME: Is `after` actually needed here?
  DeclarationFragments after;
  fragments
      .append(getFragmentsForType(func->getReturnType(), func->getASTContext(),
                                  after))
      .appendSpace()
      .append(func->getName(), DeclarationFragments::Identifier)
      .append(std::move(after));

  fragments.append("(", DeclarationFragments::Text);
  for (unsigned i = 0, end = func->getNumParams(); i != end; ++i) {
    if (i)
      fragments.append(", ", DeclarationFragments::Text);
    fragments.append(getFragmentsForParam(func->getParamDecl(i)));
  }
  fragments.append(")", DeclarationFragments::Text);

  // FIXME: Handle exception specifiers: throw, noexcept
  return fragments;
}

FunctionSignature
DeclarationFragmentsBuilder::getFunctionSignature(const FunctionDecl *func) {
  FunctionSignature signature;

  for (const auto *param : func->parameters()) {
    const auto name = param->getName();
    const auto fragments = getFragmentsForParam(param);

    signature.addParameter(name, fragments);
  }

  DeclarationFragments after;
  DeclarationFragments returns =
    getFragmentsForType(func->getReturnType(), func->getASTContext(), after);
  returns.append(std::move(after));

  signature.setReturnType(returns);

  return signature;
}

DeclarationFragments
DeclarationFragmentsBuilder::getFragmentsForEnum(const EnumDecl *enumDecl) {
  // If there's a typedef for this enum just use the declaration fragments of
  // the typedef decl.
  if (enumDecl->getTypedefNameForAnonDecl())
    return getFragmentsForTypedef(enumDecl->getTypedefNameForAnonDecl());

  DeclarationFragments fragments, after;
  fragments.append("enum", DeclarationFragments::Keyword);

  if (!enumDecl->getName().empty())
    fragments.appendSpace().append(enumDecl->getName(),
                                   DeclarationFragments::Identifier);

  QualType integerType = enumDecl->getIntegerType();
  if (!integerType.isNull())
    fragments.append(": ", DeclarationFragments::Text)
        .append(
            getFragmentsForType(integerType, enumDecl->getASTContext(), after))
        .append(std::move(after));

  return fragments;
}

DeclarationFragments DeclarationFragmentsBuilder::getFragmentsForEnumConstant(
    const EnumConstantDecl *enumConstDecl) {
  DeclarationFragments fragments;
  return fragments.append(enumConstDecl->getName(),
                          DeclarationFragments::Identifier);
}

DeclarationFragments DeclarationFragmentsBuilder::getFragmentsForObjCInterface(
    const ObjCInterfaceDecl *interface) {
  DeclarationFragments fragments;
  fragments.append("@interface", DeclarationFragments::Keyword)
      .appendSpace()
      .append(interface->getName(), DeclarationFragments::Identifier);

  if (const ObjCInterfaceDecl *superClass = interface->getSuperClass()) {
    SmallString<128> superUSR;
    clang::index::generateUSRForDecl(superClass, superUSR);
    fragments.append(" : ", DeclarationFragments::Text)
        .append(superClass->getName(), DeclarationFragments::TypeIdentifier,
                superUSR);
  }

  return fragments;
}

DeclarationFragments
DeclarationFragmentsBuilder::getFragmentsForField(const FieldDecl *field) {
  DeclarationFragments after;
  return getFragmentsForType(field->getType(), field->getASTContext(), after)
      .appendSpace()
      .append(field->getName(), DeclarationFragments::Identifier)
      .append(std::move(after));
}

DeclarationFragments
DeclarationFragmentsBuilder::getFragmentsForParam(const ParmVarDecl *param) {
  DeclarationFragments fragments, after;

  QualType paramType =
      param->getTypeSourceInfo()
          ? param->getTypeSourceInfo()->getType()
          : param->getASTContext().getUnqualifiedObjCPointerType(
                param->getType());

  DeclarationFragments type =
      getFragmentsForType(paramType, param->getASTContext(), after);

  if (param->isObjCMethodParameter())
    fragments.append("(", DeclarationFragments::Text)
        .append(std::move(type))
        .append(")", DeclarationFragments::Text);
  else
    fragments.append(std::move(type)).appendSpace();

  return fragments
      .append(param->getName(), DeclarationFragments::InternalParameter)
      .append(std::move(after));
}

DeclarationFragments DeclarationFragmentsBuilder::getFragmentsForObjCMethod(
    const ObjCMethodDecl *method) {
  DeclarationFragments fragments, after;
  if (method->isClassMethod())
    fragments.append("+ ", DeclarationFragments::Text);
  else if (method->isInstanceMethod())
    fragments.append("- ", DeclarationFragments::Text);

  fragments.append("(", DeclarationFragments::Text)
      .append(getFragmentsForType(method->getReturnType(),
                                  method->getASTContext(), after))
      .append(std::move(after))
      .append(")", DeclarationFragments::Text);

  Selector selector = method->getSelector();
  if (selector.getNumArgs() == 0)
    fragments.appendSpace().append(selector.getNameForSlot(0),
                                   DeclarationFragments::Identifier);

  for (unsigned i = 0, end = method->param_size(); i != end; ++i) {
    fragments.appendSpace()
        .append(selector.getNameForSlot(i),
                i == 0 ? DeclarationFragments::Identifier
                       : DeclarationFragments::ExternalParameter)
        .append(":", DeclarationFragments::Text);

    const ParmVarDecl *param = method->getParamDecl(i);
    fragments.append(getFragmentsForParam(param));
  }

  return fragments;
}

FunctionSignature DeclarationFragmentsBuilder::getSignatureForObjCMethod(
    const ObjCMethodDecl *method) {
  FunctionSignature signature;

  DeclarationFragments returnType, after;
  returnType
      .append(getFragmentsForType(method->getReturnType(),
                                  method->getASTContext(), after))
      .append(std::move(after));
  signature.setReturnType(returnType);

  for (const auto *param : method->parameters()) {
    auto name = param->getName();
    signature.addParameter(name, getFragmentsForParam(param));
  }

  return signature;
}

DeclarationFragments DeclarationFragmentsBuilder::getFragmentsForObjCProperty(
    const ObjCPropertyDecl *property) {
  DeclarationFragments fragments, after;

  fragments.append("@property", DeclarationFragments::Keyword);

  const auto attributes = property->getPropertyAttributes();
  if (attributes != ObjCPropertyAttribute::kind_noattr) {
    bool first = true;
    fragments.append(" (", DeclarationFragments::Text);
    auto renderAttribute = [&](ObjCPropertyAttribute::Kind kind,
                               StringRef spelling, StringRef arg = "",
                               DeclarationFragments::FragmentKind argKind =
                                   DeclarationFragments::Identifier) {
      if ((attributes & kind) && !spelling.empty()) {
        if (!first)
          fragments.append(", ", DeclarationFragments::Text);
        fragments.append(spelling, DeclarationFragments::Keyword);
        if (!arg.empty())
          fragments.append("=", DeclarationFragments::Text)
              .append(arg, argKind);
        first = false;
      }
    };

    renderAttribute(ObjCPropertyAttribute::kind_class, "class");
    renderAttribute(ObjCPropertyAttribute::kind_direct, "direct");
    renderAttribute(ObjCPropertyAttribute::kind_nonatomic, "nonatomic");
    renderAttribute(ObjCPropertyAttribute::kind_atomic, "atomic");
    renderAttribute(ObjCPropertyAttribute::kind_assign, "assign");
    renderAttribute(ObjCPropertyAttribute::kind_retain, "retain");
    renderAttribute(ObjCPropertyAttribute::kind_strong, "strong");
    renderAttribute(ObjCPropertyAttribute::kind_copy, "copy");
    renderAttribute(ObjCPropertyAttribute::kind_weak, "weak");
    renderAttribute(ObjCPropertyAttribute::kind_unsafe_unretained,
                    "unsafe_unretained");
    renderAttribute(ObjCPropertyAttribute::kind_readwrite, "readwrite");
    renderAttribute(ObjCPropertyAttribute::kind_readonly, "readonly");
    renderAttribute(ObjCPropertyAttribute::kind_getter, "getter",
                    property->getGetterName().getAsString());
    renderAttribute(ObjCPropertyAttribute::kind_setter, "setter",
                    property->getSetterName().getAsString());

    if (attributes & ObjCPropertyAttribute::kind_nullability) {
      QualType type = property->getType();
      if (const auto nullability =
              AttributedType::stripOuterNullability(type)) {
        if (!first)
          fragments.append(", ", DeclarationFragments::Text);
        if (*nullability == NullabilityKind::Unspecified &&
            (attributes & ObjCPropertyAttribute::kind_null_resettable))
          fragments.append("null_resettable", DeclarationFragments::Keyword);
        else
          fragments.append(
              getNullabilitySpelling(*nullability, /*isContextSensitive=*/true),
              DeclarationFragments::Keyword);
        first = false;
      }
    }

    fragments.append(")", DeclarationFragments::Text);
  }

  return fragments.appendSpace()
      .append(getFragmentsForType(property->getType(),
                                  property->getASTContext(), after))
      .append(property->getName(), DeclarationFragments::Identifier)
      .append(std::move(after));
}

DeclarationFragments DeclarationFragmentsBuilder::getFragmentsForObjCCategory(
    const ObjCCategoryDecl *category) {
  DeclarationFragments fragments;

  SmallString<128> interfaceUSR;
  clang::index::generateUSRForDecl(category->getClassInterface(), interfaceUSR);

  fragments.append("@interface", DeclarationFragments::Keyword)
      .appendSpace()
      .append(category->getClassInterface()->getName(),
              DeclarationFragments::TypeIdentifier, interfaceUSR)
      .append(" (", DeclarationFragments::Text)
      .append(category->getName(), DeclarationFragments::Identifier)
      .append(")", DeclarationFragments::Text);

  return fragments;
}

DeclarationFragments DeclarationFragmentsBuilder::getFragmentsForObjCProtocol(
    const ObjCProtocolDecl *protocol) {
  DeclarationFragments fragments;
  fragments.append("@protocol", DeclarationFragments::Keyword)
      .appendSpace()
      .append(protocol->getName(), DeclarationFragments::Identifier);

  if (!protocol->protocols().empty()) {
    fragments.append("<", DeclarationFragments::Text);
    for (ObjCProtocolDecl::protocol_iterator it = protocol->protocol_begin();
         it != protocol->protocol_end(); it++) {
      if (it != protocol->protocol_begin())
        fragments.append(", ", DeclarationFragments::Text);
      fragments.append((*it)->getName(), DeclarationFragments::Keyword);
    }
    fragments.append(">", DeclarationFragments::Text);
  }

  return fragments;
}

DeclarationFragments DeclarationFragmentsBuilder::getFragmentsForTypedef(
    const TypedefNameDecl *typedefDecl) {
  DeclarationFragments fragments, after;
  fragments.append("typedef", DeclarationFragments::Keyword)
      .appendSpace()
      .append(getFragmentsForType(typedefDecl->getUnderlyingType(),
                                  typedefDecl->getASTContext(), after))
      .append(std::move(after))
      .appendSpace()
      .append(typedefDecl->getName(), DeclarationFragments::Identifier);

  return fragments;
}

DeclarationFragments
DeclarationFragmentsBuilder::getFragmentsForStruct(const RecordDecl *record) {
  // If there's a typedef for this struct just use the declaration fragments of
  // the typedef decl.
  if (record->getTypedefNameForAnonDecl())
    return getFragmentsForTypedef(record->getTypedefNameForAnonDecl());

  DeclarationFragments fragments;
  fragments.append("struct", DeclarationFragments::Keyword);

  if (!record->getName().empty())
    fragments.appendSpace().append(record->getName(),
                                   DeclarationFragments::Identifier);

  return fragments;
}

// Subheading of a symbol defaults to its name.
DeclarationFragments
DeclarationFragmentsBuilder::getSubHeading(const NamedDecl *decl) {
  DeclarationFragments fragments;
  if (!decl->getName().empty())
    fragments.append(decl->getName(), DeclarationFragments::Identifier);
  return fragments;
}

// Subheading of an Objective-C method is a '+' or '-' sign indicating whether
// it's a class method or an instance method, followed by the selector name.
DeclarationFragments
DeclarationFragmentsBuilder::getSubHeading(const ObjCMethodDecl *method) {
  DeclarationFragments fragments;
  if (method->isClassMethod())
    fragments.append("+ ", DeclarationFragments::Text);
  else if (method->isInstanceMethod())
    fragments.append("- ", DeclarationFragments::Text);

  return fragments.append(method->getNameAsString(),
                          DeclarationFragments::Identifier);
}

TAPI_NAMESPACE_INTERNAL_END
