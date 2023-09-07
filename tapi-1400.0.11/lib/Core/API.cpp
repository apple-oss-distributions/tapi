//===- lib/Core/API.cpp - TAPI API ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "tapi/Core/API.h"
#include "tapi/Core/APIVisitor.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"

using namespace llvm;
using namespace clang;

TAPI_NAMESPACE_INTERNAL_BEGIN

APILoc::APILoc(std::string file, unsigned line, unsigned col)
    : file(file), line(line), col(col) {}

APILoc::APILoc(StringRef file, unsigned line, unsigned col)
    : file(file.str()), line(line), col(col) {}

bool APILoc::isInvalid() const {
  if (loc)
    return loc->isInvalid();
  else if (file.empty())
    return true;

  return false;
}

StringRef APILoc::getFilename() const {
  if (loc)
    return loc->getFilename();
  return file;
}

unsigned APILoc::getLine() const {
  if (loc)
    return loc->getLine();
  return line;
}

unsigned APILoc::getColumn() const {
  if (loc)
    return loc->getColumn();
  return col;
}

clang::PresumedLoc APILoc::getPresumedLoc() const {
  assert(loc && "must have an underlying PresumedLoc");
  return *loc;
}

SymbolInfo SymbolInfo::copied(API &api) {
  return {api.copyString(name), api.copyString(usr),
          api.copyString(sourceModule)};
}

SymbolInfo SymbolInfo::copiedInto(llvm::BumpPtrAllocator &allocator) {
  return {
      API::copyStringInto(name, allocator),
      API::copyStringInto(usr, allocator),
      API::copyStringInto(sourceModule, allocator),
  };
}

DeclarationFragments &DeclarationFragments::appendSpace() {
  if (!fragments.empty()) {
    auto last = fragments.back();
    if (last.kind == FragmentKind::Text) {
      if (last.spelling.back() != ' ') {
        last.spelling.push_back(' ');
      }
    } else {
      append(" ", FragmentKind::Text);
    }
  }

  return *this;
}

const char *DeclarationFragments::getFragmentKindString(
    DeclarationFragments::FragmentKind kind) {
  switch (kind) {
  case DeclarationFragments::Keyword:
    return "keyword";
  case DeclarationFragments::Identifier:
    return "identifier";
  case DeclarationFragments::StringLiteral:
    return "string";
  case DeclarationFragments::NumericLiteral:
    return "number";
  case DeclarationFragments::Text:
    return "text";
  case DeclarationFragments::TypeIdentifier:
    return "typeIdentifier";
  case DeclarationFragments::InternalParameter:
    return "internalParam";
  case DeclarationFragments::ExternalParameter:
    return "externalParam";
  case DeclarationFragments::Unknown:
    return "unknown";
  }

  llvm_unreachable("Unhandled FragmentKind");
}

DeclarationFragments::FragmentKind
DeclarationFragments::parseFragmentKindFromString(StringRef s) {
  return llvm::StringSwitch<FragmentKind>(s)
      .Case("keyword", DeclarationFragments::Keyword)
      .Case("identifier", DeclarationFragments::Identifier)
      .Case("string", DeclarationFragments::StringLiteral)
      .Case("number", DeclarationFragments::NumericLiteral)
      .Case("text", DeclarationFragments::Text)
      .Case("typeIdentifier", DeclarationFragments::TypeIdentifier)
      .Case("internalParam", DeclarationFragments::InternalParameter)
      .Case("externalParam", DeclarationFragments::ExternalParameter)
      .Default(DeclarationFragments::Unknown);
}

APIRecord *APIRecord::create(BumpPtrAllocator &allocator, StringRef name,
                             StringRef declName, StringRef usr,
                             APILinkage linkage, APIFlags flags, APILoc loc,
                             const AvailabilityInfo &availability,
                             APIAccess access, DocComment docComment,
                             DeclarationFragments declarationFragments,
                             DeclarationFragments subHeading,
                             const Decl *decl) {
  return new (allocator) APIRecord{name,
                                   declName,
                                   usr,
                                   loc,
                                   decl,
                                   availability,
                                   linkage,
                                   flags,
                                   access,
                                   docComment,
                                   declarationFragments,
                                   subHeading};
}

MacroDefinitionRecord *
MacroDefinitionRecord::create(llvm::BumpPtrAllocator &allocator, StringRef name,
                              StringRef usr, APILoc loc, APIAccess access,
                              DeclarationFragments declarationFragments) {
  return new (allocator)
      MacroDefinitionRecord{name, usr, loc, access, declarationFragments};
}

GlobalRecord *GlobalRecord::create(
    BumpPtrAllocator &allocator, StringRef name, StringRef declName,
    StringRef usr, APILinkage linkage, APIFlags flags, APILoc loc,
    const AvailabilityInfo &availability, APIAccess access,
    DocComment docComment, DeclarationFragments declarationFragments,
    DeclarationFragments subHeading, FunctionSignature functionSignature,
    const Decl *decl, GVKind kind) {

  return new (allocator) GlobalRecord{name,
                                      declName,
                                      usr,
                                      flags,
                                      loc,
                                      availability,
                                      access,
                                      docComment,
                                      declarationFragments,
                                      subHeading,
                                      functionSignature,
                                      decl,
                                      kind,
                                      linkage};
}

StructRecord *StructRecord::create(BumpPtrAllocator &allocator, StringRef name,
                                   StringRef usr, APILoc loc,
                                   const AvailabilityInfo &availability,
                                   APIAccess access, DocComment docComment,
                                   DeclarationFragments declarationFragments,
                                   DeclarationFragments subHeading,
                                   const Decl *decl) {
  return new (allocator) StructRecord{name,
                                      usr,
                                      loc,
                                      availability,
                                      access,
                                      docComment,
                                      declarationFragments,
                                      subHeading,
                                      decl};
}

StructFieldRecord *
StructFieldRecord::create(BumpPtrAllocator &allocator, StringRef name,
                          StringRef declName, StringRef usr, APILoc loc,
                          const AvailabilityInfo &availability,
                          APIAccess access, DocComment docComment,
                          DeclarationFragments declarationFragments,
                          DeclarationFragments subHeading, const Decl *decl) {
  return new (allocator) StructFieldRecord{
      name,         declName, usr,        loc,
      availability, access,   docComment, declarationFragments,
      subHeading,   decl};
}

EnumRecord *EnumRecord::create(BumpPtrAllocator &allocator, StringRef name,
                               StringRef declName, StringRef usr, APILoc loc,
                               const AvailabilityInfo &availability,
                               APIAccess access, DocComment docComment,
                               DeclarationFragments declarationFragments,
                               DeclarationFragments subHeading,
                               const Decl *decl) {
  return new (allocator)
      EnumRecord{name,         declName, usr,        loc,
                 availability, access,   docComment, declarationFragments,
                 subHeading,   decl};
}

EnumConstantRecord *
EnumConstantRecord::create(BumpPtrAllocator &allocator, StringRef name,
                           StringRef declName, StringRef usr, APILoc loc,
                           const AvailabilityInfo &availability,
                           APIAccess access, DocComment docComment,
                           DeclarationFragments declarationFragments,
                           DeclarationFragments subHeading, const Decl *decl) {
  return new (allocator) EnumConstantRecord{
      name,         declName, usr,        loc,
      availability, access,   docComment, declarationFragments,
      subHeading,   decl};
}

ObjCMethodRecord *ObjCMethodRecord::create(
    BumpPtrAllocator &allocator, StringRef name, StringRef usr, APILoc loc,
    const AvailabilityInfo &availability, APIAccess access,
    bool isInstanceMethod, bool isOptional, bool isDynamic,
    DocComment docComment, DeclarationFragments declarationFragments,
    DeclarationFragments subHeading, FunctionSignature signature,
    const Decl *decl) {
  return new (allocator) ObjCMethodRecord{name,       usr,
                                          loc,        availability,
                                          access,     isInstanceMethod,
                                          isOptional, isDynamic,
                                          docComment, declarationFragments,
                                          subHeading, signature,
                                          decl};
}

ObjCPropertyRecord *ObjCPropertyRecord::create(
    BumpPtrAllocator &allocator, StringRef name, StringRef usr,
    StringRef getterName, StringRef setterName, APILoc loc,
    const AvailabilityInfo &availability, APIAccess access,
    AttributeKind attributes, bool isOptional, DocComment docComment,
    DeclarationFragments declarationFragments, DeclarationFragments subHeading,
    const Decl *decl) {
  return new (allocator) ObjCPropertyRecord{name,
                                            usr,
                                            getterName,
                                            setterName,
                                            loc,
                                            availability,
                                            access,
                                            attributes,
                                            isOptional,
                                            docComment,
                                            declarationFragments,
                                            subHeading,
                                            decl};
}

ObjCInstanceVariableRecord *ObjCInstanceVariableRecord::create(
    BumpPtrAllocator &allocator, StringRef name, StringRef usr,
    APILinkage linkage, APILoc loc, const AvailabilityInfo &availability,
    APIAccess access, AccessControl accessControl, DocComment docComment,
    DeclarationFragments declarationFragments, DeclarationFragments subHeading,
    const Decl *decl) {
  return new (allocator) ObjCInstanceVariableRecord{
      name,          usr,          linkage,
      loc,           availability, access,
      accessControl, docComment,   declarationFragments,
      subHeading,    decl};
}

ObjCInterfaceRecord *ObjCInterfaceRecord::create(
    BumpPtrAllocator &allocator, StringRef name, StringRef declName,
    StringRef usr, APILinkage linkage, APILoc loc,
    const AvailabilityInfo &availability, APIAccess access,
    SymbolInfo superClass, DocComment docComment,
    DeclarationFragments declarationFragments, DeclarationFragments subHeading,
    const Decl *decl) {
  return new (allocator) ObjCInterfaceRecord{
      name,         declName, usr,        linkage,    loc,
      availability, access,   superClass, docComment, declarationFragments,
      subHeading,   decl};
}

ObjCCategoryRecord *
ObjCCategoryRecord::create(BumpPtrAllocator &allocator, SymbolInfo interface,
                           StringRef name, StringRef usr, APILoc loc,
                           const AvailabilityInfo &availability,
                           APIAccess access, DocComment docComment,
                           DeclarationFragments declarationFragments,
                           DeclarationFragments subHeading, const Decl *decl) {
  return new (allocator)
      ObjCCategoryRecord{interface,    name,   usr,        loc,
                         availability, access, docComment, declarationFragments,
                         subHeading,   decl};
}

ObjCProtocolRecord *ObjCProtocolRecord::create(
    BumpPtrAllocator &allocator, StringRef name, StringRef usr, APILoc loc,
    const AvailabilityInfo &availability, APIAccess access,
    DocComment docComment, DeclarationFragments declarationFragments,
    DeclarationFragments subHeading, const Decl *decl) {
  return new (allocator) ObjCProtocolRecord{name,
                                            usr,
                                            loc,
                                            availability,
                                            access,
                                            docComment,
                                            declarationFragments,
                                            subHeading,
                                            decl};
}

TypedefRecord *
TypedefRecord::create(llvm::BumpPtrAllocator &allocator, StringRef name,
                      StringRef usr, APILoc loc,
                      const AvailabilityInfo &availability, APIAccess access,
                      SymbolInfo underlyingType, DocComment docComment,
                      DeclarationFragments declarationFragment,
                      DeclarationFragments subHeading, const Decl *decl) {
  return new (allocator) TypedefRecord{name,       usr,
                                       loc,        availability,
                                       access,     underlyingType,
                                       docComment, declarationFragment,
                                       subHeading, decl};
}

bool API::updateAPIAccess(APIRecord *record, APIAccess access) {
  if (record->access <= access)
    return false;

  record->access = access;
  return true;
}

bool API::updateAPILinkage(APIRecord *record, APILinkage linkage) {
  if (record->linkage >= linkage)
    return false;

  record->linkage = linkage;
  return true;
}

MacroDefinitionRecord *
API::addMacroDefinition(StringRef name, StringRef usr, APILoc loc,
                        APIAccess access,
                        DeclarationFragments declarationFragments) {
  name = copyString(name);
  auto result = macros.insert({name, nullptr});
  if (result.second) {
    usr = copyString(usr);
    auto *record = MacroDefinitionRecord::create(allocator, name, usr, loc,
                                                 access, declarationFragments);
    result.first->second = record;
  }
  API::updateAPIAccess(result.first->second, access);
  return result.first->second;
}

GlobalRecord *API::addGlobal(StringRef name, StringRef declName, StringRef usr,
                             APILoc loc, const AvailabilityInfo &availability,
                             APIAccess access, DocComment docComment,
                             DeclarationFragments declarationFragments,
                             DeclarationFragments subHeading,
                             FunctionSignature functionSignature,
                             const Decl *decl, GVKind kind, APILinkage linkage,
                             bool isWeakDefined, bool isThreadLocal) {
  auto flags = APIFlags::None;
  if (isWeakDefined)
    flags |= APIFlags::WeakDefined;
  if (isThreadLocal)
    flags |= APIFlags::ThreadLocalValue;
  return addGlobal(name, declName, usr, flags, loc, availability, access,
                   docComment, declarationFragments, subHeading,
                   functionSignature, decl, kind, linkage);
}

GlobalRecord *API::addGlobal(
    StringRef name, StringRef declName, StringRef usr, APIFlags flags,
    APILoc loc, const AvailabilityInfo &availability, APIAccess access,
    DocComment docComment, DeclarationFragments declarationFragments,
    DeclarationFragments subHeading, FunctionSignature functionSignature,
    const Decl *decl, GVKind kind, APILinkage linkage) {
  name = copyString(name);
  auto result = globals.insert({name, nullptr});
  if (result.second) {
    usr = copyString(usr);
    declName = copyString(declName);
    auto *record = GlobalRecord::create(
        allocator, name, declName, usr, linkage, flags, loc, availability,
        access, docComment, declarationFragments, subHeading, functionSignature,
        decl, kind);
    result.first->second = record;
  }
  API::updateAPIAccess(result.first->second, access);
  API::updateAPILinkage(result.first->second, linkage);
  // TODO: diagnose kind difference.
  return result.first->second;
}

GlobalRecord *API::addGlobalVariable(StringRef name, StringRef declName,
                                     StringRef usr, APILoc loc,
                                     const AvailabilityInfo &availability,
                                     APIAccess access, DocComment docComment,
                                     DeclarationFragments declarationFragments,
                                     DeclarationFragments subHeading,
                                     const Decl *decl, APILinkage linkage,
                                     bool isWeakDefined, bool isThreadLocal) {
  return addGlobal(name, declName, usr, loc, availability, access, docComment,
                   declarationFragments, subHeading, {}, decl, GVKind::Variable,
                   linkage, isWeakDefined, isThreadLocal);
}

GlobalRecord *API::addFunction(
    StringRef name, StringRef declName, StringRef usr, APILoc loc,
    const AvailabilityInfo &availability, APIAccess access,
    DocComment docComment, DeclarationFragments declarationFragments,
    DeclarationFragments subHeading, FunctionSignature functionSignature,
    const Decl *decl, APILinkage linkage, bool isWeakDefined) {
  return addGlobal(name, declName, usr, loc, availability, access, docComment,
                   declarationFragments, subHeading, functionSignature, decl,
                   GVKind::Function, linkage, isWeakDefined);
}

EnumRecord *API::addEnum(StringRef name, StringRef declName, StringRef usr,
                         APILoc loc, const AvailabilityInfo &availability,
                         APIAccess access, DocComment docComment,
                         DeclarationFragments declarationFragments,
                         DeclarationFragments subHeading, const Decl *decl) {
  usr = copyString(usr);
  // Use USR as the key, as all anonymous enums have the same name.
  auto result = enums.insert({usr, nullptr});
  if (result.second) {
    name = copyString(name);
    declName = copyString(declName);
    auto *record = EnumRecord::create(allocator, name, declName, usr, loc,
                                      availability, access, docComment,
                                      declarationFragments, subHeading, decl);
    result.first->second = record;
  }
  return result.first->second;
}

EnumConstantRecord *API::addEnumConstant(
    EnumRecord *record, StringRef name, StringRef declName, StringRef usr,
    APILoc loc, const AvailabilityInfo &availability, APIAccess access,
    DocComment docComment, DeclarationFragments declarationFragments,
    DeclarationFragments subHeading, const Decl *decl) {
  name = copyString(name);
  declName = copyString(declName);
  usr = copyString(usr);
  auto *constant = EnumConstantRecord::create(
      allocator, name, declName, usr, loc, availability, access, docComment,
      declarationFragments, subHeading, decl);
  record->constants.push_back(constant);
  return constant;
}

ObjCInterfaceRecord *
API::addObjCInterface(StringRef name, StringRef declName, StringRef usr,
                      APILoc loc, const AvailabilityInfo &availability,
                      APIAccess access, APILinkage linkage,
                      SymbolInfo superClass, DocComment docComment,
                      DeclarationFragments declarationFragments,
                      DeclarationFragments subHeading, const Decl *decl) {
  name = copyString(name);
  auto result = interfaces.insert({name, nullptr});
  if (result.second) {
    declName = copyString(declName);
    usr = copyString(usr);
    superClass.name = copyString(superClass.name);
    superClass.usr = copyString(superClass.usr);
    superClass.sourceModule = copyString(superClass.sourceModule);
    auto *record = ObjCInterfaceRecord::create(
        allocator, name, declName, usr, linkage, loc, availability, access,
        superClass, docComment, declarationFragments, subHeading, decl);
    result.first->second = record;
  }
  return result.first->second;
}

ObjCCategoryRecord *
API::addObjCCategory(SymbolInfo interface, StringRef name, StringRef usr,
                     APILoc loc, const AvailabilityInfo &availability,
                     APIAccess access, DocComment docComment,
                     DeclarationFragments declarationFragments,
                     DeclarationFragments subHeading, const Decl *decl) {
  interface.name = copyString(interface.name);
  name = copyString(name);
  auto result =
      categories.insert({std::make_pair(interface.name, name), nullptr});
  if (result.second) {
    usr = copyString(usr);
    interface.usr = copyString(interface.usr);
    interface.sourceModule = copyString(interface.sourceModule);
    auto *record = ObjCCategoryRecord::create(
        allocator, interface, name, usr, loc, availability, access, docComment,
        declarationFragments, subHeading, decl);
    result.first->second = record;
  }

  auto it = interfaces.find(interface.name);
  if (it != interfaces.end())
    it->second->categories.push_back(result.first->second);

  return result.first->second;
}

ObjCProtocolRecord *
API::addObjCProtocol(StringRef name, StringRef usr, APILoc loc,
                     const AvailabilityInfo &availability, APIAccess access,
                     DocComment docComment,
                     DeclarationFragments declarationFragments,
                     DeclarationFragments subHeading, const Decl *decl) {
  name = copyString(name);
  auto result = protocols.insert({name, nullptr});
  if (result.second) {
    usr = copyString(usr);
    auto *record = ObjCProtocolRecord::create(
        allocator, name, usr, loc, availability, access, docComment,
        declarationFragments, subHeading, decl);
    result.first->second = record;
  }

  return result.first->second;
}

void API::addObjCProtocol(ObjCContainerRecord *record, SymbolInfo protocol) {
  protocol.name = copyString(protocol.name);
  protocol.usr = copyString(protocol.usr);
  protocol.sourceModule = copyString(protocol.sourceModule);
  record->protocols.push_back(protocol);
}

ObjCMethodRecord *
API::addObjCMethod(ObjCContainerRecord *record, StringRef name, StringRef usr,
                   APILoc loc, const AvailabilityInfo &availability,
                   APIAccess access, bool isInstanceMethod, bool isOptional,
                   bool isDynamic, DocComment docComment,
                   DeclarationFragments declarationFragments,
                   DeclarationFragments subHeading, FunctionSignature signature,
                   const Decl *decl) {
  name = copyString(name);
  usr = copyString(usr);
  auto *method = ObjCMethodRecord::create(
      allocator, name, usr, loc, availability, access, isInstanceMethod,
      isOptional, isDynamic, docComment, declarationFragments, subHeading,
      signature, decl);
  record->methods.push_back(method);
  return method;
}

ObjCPropertyRecord *
API::addObjCProperty(ObjCContainerRecord *record, StringRef name, StringRef usr,
                     StringRef getterName, StringRef setterName, APILoc loc,
                     const AvailabilityInfo &availability, APIAccess access,
                     ObjCPropertyRecord::AttributeKind attributes,
                     bool isOptional, DocComment docComment,
                     DeclarationFragments declarationFragments,
                     DeclarationFragments subHeading, const Decl *decl) {
  name = copyString(name);
  usr = copyString(usr);
  getterName = copyString(getterName);
  setterName = copyString(setterName);
  auto *property = ObjCPropertyRecord::create(
      allocator, name, usr, getterName, setterName, loc, availability, access,
      attributes, isOptional, docComment, declarationFragments, subHeading,
      decl);
  record->properties.push_back(property);
  return property;
}

ObjCInstanceVariableRecord *API::addObjCInstanceVariable(
    ObjCContainerRecord *record, StringRef name, StringRef usr, APILoc loc,
    const AvailabilityInfo &availability, APIAccess access,
    ObjCInstanceVariableRecord::AccessControl accessControl, APILinkage linkage,
    DocComment docComment, DeclarationFragments declarationFragments,
    DeclarationFragments subHeading, const Decl *decl) {
  name = copyString(name);
  usr = copyString(usr);
  auto *ivar = ObjCInstanceVariableRecord::create(
      allocator, name, usr, linkage, loc, availability, access, accessControl,
      docComment, declarationFragments, subHeading, decl);
  record->ivars.push_back(ivar);
  return ivar;
}

StructRecord *API::addStruct(StringRef name, StringRef usr, APILoc loc,
                             const AvailabilityInfo &availability,
                             APIAccess access, DocComment docComment,
                             DeclarationFragments declarationFragments,
                             DeclarationFragments subHeading,
                             const Decl *decl) {
  name = copyString(name);
  auto result = structs.insert({name, nullptr});
  if (result.second) {
    usr = copyString(usr);
    auto *record = StructRecord::create(allocator, name, usr, loc, availability,
                                        access, docComment,
                                        declarationFragments, subHeading, decl);
    result.first->second = record;
  }
  return result.first->second;
}

StructFieldRecord *API::addStructField(
    StructRecord *record, StringRef name, StringRef declName, StringRef usr,
    APILoc loc, const AvailabilityInfo &availability, APIAccess access,
    DocComment docComment, DeclarationFragments declarationFragments,
    DeclarationFragments subHeading, const Decl *decl) {
  const auto it = find_if(record->fields, [&](const StructFieldRecord *field) {
    return field->name.equals(name);
  });
  if (it != record->fields.end())
    return *it;

  name = copyString(name);
  declName = copyString(declName);
  usr = copyString(usr);
  auto *field = StructFieldRecord::create(
      allocator, name, declName, usr, loc, availability, access, docComment,
      declarationFragments, subHeading, decl);
  record->fields.push_back(field);
  return field;
}

TypedefRecord *API::addTypeDef(StringRef name, StringRef usr, APILoc loc,
                               const AvailabilityInfo &availability,
                               APIAccess access, SymbolInfo underlyingType,
                               DocComment docComment,
                               DeclarationFragments declarationFragments,
                               DeclarationFragments subHeading,
                               const Decl *decl) {
  name = copyString(name);
  usr = copyString(usr);
  auto result = typeDefs.insert({name, nullptr});
  if (result.second) {
    underlyingType.name = copyString(underlyingType.name);
    underlyingType.usr = copyString(underlyingType.usr);
    underlyingType.sourceModule = copyString(underlyingType.sourceModule);
    auto *record = TypedefRecord::create(
        allocator, name, usr, loc, availability, access, underlyingType,
        docComment, declarationFragments, subHeading, decl);
    result.first->second = record;
  }
  return result.first->second;
}

const GlobalRecord *API::findGlobalVariable(StringRef name) const {
  auto it = globals.find(name);
  if (it != globals.end() && it->second->kind == GVKind::Variable)
    return it->second;
  return nullptr;
}

const GlobalRecord *API::findFunction(StringRef name) const {
  auto it = globals.find(name);
  if (it != globals.end() && it->second->kind == GVKind::Function)
    return it->second;
  return nullptr;
}

const TypedefRecord *API::findTypeDef(StringRef name) const {
  auto it = typeDefs.find(name);
  if (it != typeDefs.end())
    return it->second;
  return nullptr;
}

const EnumRecord *API::findEnum(StringRef usr) const {
  auto it = enums.find(usr);
  if (it != enums.end())
    return it->second;
  return nullptr;
}

const ObjCInterfaceRecord *API::findObjCInterface(StringRef name) const {
  auto it = interfaces.find(name);
  if (it != interfaces.end())
    return it->second;
  return nullptr;
}

const ObjCProtocolRecord *API::findObjCProtocol(StringRef name) const {
  auto it = protocols.find(name);
  if (it != protocols.end())
    return it->second;
  return nullptr;
}

const ObjCCategoryRecord *API::findObjCCategory(StringRef interfaceName,
                                                StringRef name) const {
  auto it = categories.find({interfaceName, name});
  if (it != categories.end())
    return it->second;
  return nullptr;
}

const StructRecord *API::findStruct(StringRef name) const {
  auto it = structs.find(name);
  if (it != structs.end())
    return it->second;
  return nullptr;
}

void API::visit(APIVisitor &visitor) const {
  for (auto &it : macros)
    visitor.visitMacroDefinition(*it.second);
  for (auto &it : typeDefs)
    visitor.visitTypeDef(*it.second);
  for (auto &it : globals)
    visitor.visitGlobal(*it.second);
  for (auto &it : enums)
    visitor.visitEnum(*it.second);
  for (auto &it : protocols)
    visitor.visitObjCProtocol(*it.second);
  for (auto &it : interfaces)
    visitor.visitObjCInterface(*it.second);
  for (auto &it : categories)
    visitor.visitObjCCategory(*it.second);
  for (auto &it : structs)
    visitor.visitStruct(*it.second);
}

void API::visit(APIMutator &visitor) {
  for (auto &it : macros)
    visitor.visitMacroDefinition(*it.second);
  for (auto &it : typeDefs)
    visitor.visitTypeDef(*it.second);
  for (auto &it : globals)
    visitor.visitGlobal(*it.second);
  for (auto &it : enums)
    visitor.visitEnum(*it.second);
  for (auto &it : protocols)
    visitor.visitObjCProtocol(*it.second);
  for (auto &it : interfaces)
    visitor.visitObjCInterface(*it.second);
  for (auto &it : categories)
    visitor.visitObjCCategory(*it.second);
  for (auto &it : structs)
    visitor.visitStruct(*it.second);
}

StringRef API::copyStringInto(StringRef string,
                              llvm::BumpPtrAllocator &allocator) {
  if (string.empty())
    return {};

  if (allocator.identifyObject(string.data()))
    return string;

  void *ptr = allocator.Allocate(string.size(), 1);
  memcpy(ptr, string.data(), string.size());
  return StringRef(reinterpret_cast<const char *>(ptr), string.size());
}

StringRef API::copyString(StringRef string) {
  return API::copyStringInto(string, allocator);
}

BinaryInfo &API::getBinaryInfo() {
  if (hasBinaryInfo())
    return *binaryInfo;

  // Allocate in bumpPtrAllocator so it has a stable address.
  binaryInfo =  new (allocator) BinaryInfo{};
  return *binaryInfo;
}

bool API::operator<(const API &other) const {
  // First, let's see if we can order them based on binaryInfo.
  // 1. Put the one with binary info first.
  if (!hasBinaryInfo() && other.hasBinaryInfo())
    return false;
  if (hasBinaryInfo() && !other.hasBinaryInfo())
    return true;

  // 2. Sort by binary kind and installName.
  if (hasBinaryInfo() && other.hasBinaryInfo()) {
    if (getBinaryInfo() != other.getBinaryInfo())
      return getBinaryInfo() < other.getBinaryInfo();
  }

  // 3. Sorted by target triple.
  // Doing string comparsion here since version matters.
  if (target.str() != other.target.str())
    return target.str() < other.target.str();

  // 4. Sort by number of APIs. Pick the one has more APIs.
  if (macros.size() != other.macros.size())
    return macros.size() > other.macros.size();
  if (globals.size() != other.globals.size())
    return globals.size() > other.globals.size();
  if (interfaces.size() != other.interfaces.size())
    return interfaces.size() > other.interfaces.size();
  if (protocols.size() != other.protocols.size())
    return protocols.size() > other.protocols.size();
  if (enums.size() != other.enums.size())
    return enums.size() > other.enums.size();

  // fallback plan. unstable ordering.
  return false;
}

TAPI_NAMESPACE_INTERNAL_END
