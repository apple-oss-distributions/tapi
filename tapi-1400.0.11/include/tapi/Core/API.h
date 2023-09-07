//===- tapi/Core/API.h - TAPI API -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the API.
///
//===----------------------------------------------------------------------===//

#ifndef TAPI_CORE_API_H
#define TAPI_CORE_API_H

#include "tapi/Core/APICommon.h"
#include "tapi/Core/AvailabilityInfo.h"
#include "tapi/Core/InterfaceFile.h"
#include "tapi/Core/LLVM.h"
#include "tapi/Core/Utils.h"
#include "tapi/Defines.h"
#include "clang/AST/DeclObjC.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/Error.h"
#include <iterator>

using clang::Decl;

namespace clang {
class RawComment;
} // end namespace clang

TAPI_NAMESPACE_INTERNAL_BEGIN

class API;

class APILoc {
public:
  APILoc() = default;
  APILoc(clang::PresumedLoc loc) : loc(loc) {}
  APILoc(std::string file, unsigned line, unsigned col);
  APILoc(StringRef file, unsigned line, unsigned col);

  bool isInvalid() const;
  StringRef getFilename() const;
  unsigned getLine() const;
  unsigned getColumn() const;
  clang::PresumedLoc getPresumedLoc() const;

private:
  llvm::Optional<clang::PresumedLoc> loc;
  std::string file;
  unsigned line;
  unsigned col;
};

class APIRange {
public:
  APIRange() = default;
  APIRange(APILoc loc) : begin(loc), end(loc) {}
  APIRange(APILoc begin, APILoc end) : begin(begin), end(end) {}

  APILoc getBegin() const { return begin; }
  APILoc getEnd() const { return end; }

private:
  APILoc begin;
  APILoc end;
};

class DocComment {
public:
  DocComment() = default;
  DocComment(const clang::RawComment *rawComment,
             const clang::SourceManager &sourceManager,
             clang::DiagnosticsEngine &diags);

  struct CommentLine {
    std::string text;
    APIRange range;

    CommentLine(StringRef text, APIRange range) : text(text), range(range) {}
    CommentLine(StringRef text, APILoc begin, APILoc end)
        : text(text), range(begin, end) {}
  };

  const std::vector<CommentLine> &getCommentLines() const {
    return commentLines;
  }

  void addCommentLine(StringRef text, APIRange range) {
    commentLines.emplace_back(text, range);
  }

private:
  std::vector<CommentLine> commentLines;
};

class DeclarationFragments {
public:
  DeclarationFragments() = default;

  enum FragmentKind : uint8_t {
    Keyword = 0,
    Identifier = 1,
    StringLiteral = 2,
    NumericLiteral = 3,
    Text = 4,
    TypeIdentifier = 5,
    InternalParameter = 6,
    ExternalParameter = 7,
    Unknown,
  };

  struct Fragment {
    std::string spelling;
    FragmentKind kind;
    std::string preciseIdentifier;

    Fragment(StringRef spelling, FragmentKind kind, StringRef preciseIdentifier)
        : spelling(spelling), kind(kind), preciseIdentifier(preciseIdentifier) {
    }
  };

  const std::vector<Fragment> &getFragments() const { return fragments; }

  DeclarationFragments &append(StringRef spelling, FragmentKind kind,
                               StringRef preciseIdentifier = "") {
    if (kind == FragmentKind::Text && !fragments.empty() &&
        fragments.back().kind == FragmentKind::Text) {
      fragments.back().spelling.append(spelling.data(), spelling.size());
    } else {
      fragments.emplace_back(spelling, kind, preciseIdentifier);
    }
    return *this;
  }

  DeclarationFragments &append(DeclarationFragments &&other) {
    fragments.insert(fragments.end(),
                     std::make_move_iterator(other.fragments.begin()),
                     std::make_move_iterator(other.fragments.end()));
    other.fragments.clear();
    return *this;
  }

  DeclarationFragments &appendSpace();

  static const char *getFragmentKindString(FragmentKind kind);
  static FragmentKind parseFragmentKindFromString(StringRef s);

private:
  std::vector<Fragment> fragments;
};

class FunctionSignature {
public:
  FunctionSignature() = default;

  struct Parameter {
    std::string name;
    DeclarationFragments declarationFragments;

    Parameter(StringRef name, DeclarationFragments declarationFragments)
        : name(name), declarationFragments(declarationFragments) {}
  };

  const std::vector<Parameter> &getParameters() const { return parameters; }
  const DeclarationFragments &getReturnType() const { return returnType; }

  FunctionSignature &addParameter(StringRef name,
                                  DeclarationFragments declarationFragments) {
    parameters.emplace_back(name, declarationFragments);
    return *this;
  }

  void setReturnType(DeclarationFragments newReturnType) {
    returnType = newReturnType;
  }

  bool empty() const {
    return parameters.empty() && returnType.getFragments().empty();
  }

private:
  std::vector<Parameter> parameters;
  DeclarationFragments returnType;
};

struct SymbolInfo {
  StringRef name;
  StringRef usr;
  StringRef sourceModule;

  SymbolInfo() = default;
  SymbolInfo(StringRef name, StringRef usr = "", StringRef sourceModule = "")
      : name(name), usr(usr), sourceModule(sourceModule) {}

  bool empty() const {
    return name.empty() && usr.empty() && sourceModule.empty();
  }

  SymbolInfo copied(API &api);
  SymbolInfo copiedInto(llvm::BumpPtrAllocator &);
};

struct APIRecord {
  StringRef name;
  StringRef declName;
  StringRef usr;
  APILoc loc;
  const Decl *decl;
  AvailabilityInfo availability;
  APILinkage linkage;
  APIFlags flags;
  APIAccess access;
  DocComment docComment;
  DeclarationFragments declarationFragments;
  DeclarationFragments subHeading;

  static APIRecord *create(llvm::BumpPtrAllocator &allocator, StringRef name,
                           StringRef declName, StringRef usr,
                           APILinkage linkage, APIFlags flags, APILoc loc,
                           const AvailabilityInfo &availability,
                           APIAccess access, DocComment docComment,
                           DeclarationFragments declarationFragments,
                           DeclarationFragments subHeading, const Decl *decl);

  bool isWeakDefined() const {
    return (flags & APIFlags::WeakDefined) == APIFlags::WeakDefined;
  }

  bool isWeakReferenced() const {
    return (flags & APIFlags::WeakReferenced) == APIFlags::WeakReferenced;
  }

  bool isThreadLocalValue() const {
    return (flags & APIFlags::ThreadLocalValue) == APIFlags::ThreadLocalValue;
  }

  bool isExternal() const { return linkage == APILinkage::External; }
  bool isExported() const { return linkage >= APILinkage::Reexported; }
  bool isReexported() const { return linkage == APILinkage::Reexported; }
};

struct MacroDefinitionRecord : APIRecord {
  MacroDefinitionRecord(StringRef name, StringRef usr, APILoc loc,
                        APIAccess access,
                        DeclarationFragments declarationFragments)
      : APIRecord({name,
                   {},
                   usr,
                   loc,
                   nullptr,
                   {},
                   APILinkage::Unknown,
                   APIFlags::None,
                   access,
                   {},
                   declarationFragments,
                   {}}) {}
  static MacroDefinitionRecord *
  create(llvm::BumpPtrAllocator &allocator, StringRef name, StringRef usr,
         APILoc loc, APIAccess access,
         DeclarationFragments declarationFragments);
};

struct StructFieldRecord : APIRecord {
  StructFieldRecord(StringRef name, StringRef declName, StringRef usr,
                    APILoc loc, const AvailabilityInfo &availability,
                    APIAccess access, DocComment docComment,
                    DeclarationFragments declarationFragments,
                    DeclarationFragments subHeading, const Decl *decl)
      : APIRecord({name, declName, usr, loc, decl, availability,
                   APILinkage::Unknown, APIFlags::None, access, docComment,
                   declarationFragments, subHeading}) {}
  static StructFieldRecord *
  create(llvm::BumpPtrAllocator &allocator, StringRef name, StringRef declName,
         StringRef usr, APILoc loc, const AvailabilityInfo &availability,
         APIAccess access, DocComment docComment,
         DeclarationFragments declarationFragments,
         DeclarationFragments subHeading, const Decl *decl);
};

struct StructRecord : APIRecord {
  std::vector<StructFieldRecord *> fields;

  StructRecord(StringRef name, StringRef usr, APILoc loc,
               const AvailabilityInfo &availability, APIAccess access,
               DocComment docComment, DeclarationFragments declarationFragments,
               DeclarationFragments subHeading, const Decl *decl)
      : APIRecord({name, StringRef{}, usr, loc, decl, availability,
                   APILinkage::Unknown, APIFlags::None, access, docComment,
                   declarationFragments, subHeading}) {}
  static StructRecord *
  create(llvm::BumpPtrAllocator &allocator, StringRef name, StringRef usr,
         APILoc loc, const AvailabilityInfo &availability, APIAccess access,
         DocComment docComment, DeclarationFragments declarationFragments,
         DeclarationFragments subHeading, const Decl *decl);
};

struct EnumConstantRecord : APIRecord {
  EnumConstantRecord(StringRef name, StringRef declName, StringRef usr,
                     APILoc loc, const AvailabilityInfo &availability,
                     APIAccess access, DocComment docComment,
                     DeclarationFragments declarationFragments,
                     DeclarationFragments subHeading, const Decl *decl)
      : APIRecord({name, declName, usr, loc, decl, availability,
                   APILinkage::Unknown, APIFlags::None, access, docComment,
                   declarationFragments, subHeading}) {}
  static EnumConstantRecord *
  create(llvm::BumpPtrAllocator &allocator, StringRef name, StringRef declName,
         StringRef usr, APILoc loc, const AvailabilityInfo &availability,
         APIAccess access, DocComment docComment,
         DeclarationFragments declarationFragments,
         DeclarationFragments subHeading, const Decl *decl);
};

struct EnumRecord : APIRecord {
  std::vector<EnumConstantRecord *> constants;

  EnumRecord(StringRef name, StringRef declName, StringRef usr, APILoc loc,
             const AvailabilityInfo &availability, APIAccess access,
             DocComment docComment, DeclarationFragments declarationFragments,
             DeclarationFragments subHeading, const Decl *decl)
      : APIRecord({name, declName, usr, loc, decl, availability,
                   APILinkage::Unknown, APIFlags::None, access, docComment,
                   declarationFragments, subHeading}) {}
  static EnumRecord *create(llvm::BumpPtrAllocator &allocator, StringRef name,
                            StringRef declName, StringRef usr, APILoc loc,
                            const AvailabilityInfo &availability,
                            APIAccess access, DocComment docComment,
                            DeclarationFragments declarationFragments,
                            DeclarationFragments subHeading, const Decl *decl);
};

enum class GVKind : uint8_t {
  Unknown = 0,
  Variable = 1,
  Function = 2,
};

struct GlobalRecord : APIRecord {
  GVKind kind;
  FunctionSignature functionSignature;

  GlobalRecord(StringRef name, StringRef declName, StringRef usr,
               APIFlags flags, APILoc loc, const AvailabilityInfo &availability,
               APIAccess access, DocComment docComment,
               DeclarationFragments declarationFragments,
               DeclarationFragments subHeading,
               FunctionSignature functionSignature, const Decl *decl,
               GVKind kind, APILinkage linkage)
      : APIRecord({name, declName, usr, loc, decl, availability, linkage, flags,
                   access, docComment, declarationFragments, subHeading}),
        kind(kind), functionSignature(functionSignature) {}

  static GlobalRecord *
  create(llvm::BumpPtrAllocator &allocator, StringRef name, StringRef declName,
         StringRef usr, APILinkage linkage, APIFlags flags, APILoc loc,
         const AvailabilityInfo &availability, APIAccess access,
         DocComment docComment, DeclarationFragments declarationFragments,
         DeclarationFragments subHeading, FunctionSignature functionSignature,
         const Decl *decl, GVKind kind);
};

struct ObjCPropertyRecord : APIRecord {
  enum AttributeKind : unsigned {
    NoAttr = 0,
    ReadOnly = 1,
    Class = 1 << 1,
    Dynamic = 1 << 2,
  };

  AttributeKind attributes;
  StringRef getterName;
  StringRef setterName;
  bool isOptional;

  ObjCPropertyRecord(StringRef name, StringRef usr, StringRef getterName,
                     StringRef setterName, APILoc loc,
                     const AvailabilityInfo &availability, APIAccess access,
                     AttributeKind attributes, bool isOptional,
                     DocComment docComment,
                     DeclarationFragments declarationFragments,
                     DeclarationFragments subHeading, const Decl *decl)
      : APIRecord({name, StringRef{}, usr, loc, decl, availability,
                   APILinkage::Unknown, APIFlags::None, access, docComment,
                   declarationFragments, subHeading}),
        attributes(attributes), getterName(getterName), setterName(setterName),
        isOptional(isOptional) {}

  static ObjCPropertyRecord *
  create(llvm::BumpPtrAllocator &allocator, StringRef name, StringRef usr,
         StringRef getterName, StringRef setterName, APILoc loc,
         const AvailabilityInfo &availability, APIAccess access,
         AttributeKind attributes, bool isOptional, DocComment docComment,
         DeclarationFragments declarationFragments,
         DeclarationFragments subHeading, const Decl *decl);

  bool isReadOnly() const { return attributes & ReadOnly; }
  bool isDynamic() const { return attributes & Dynamic; }
  bool isClassProperty() const { return attributes & Class; }
};

struct ObjCInstanceVariableRecord : APIRecord {
  using AccessControl = clang::ObjCIvarDecl::AccessControl;
  AccessControl accessControl;

  ObjCInstanceVariableRecord(StringRef name, StringRef usr, APILinkage linkage,
                             APILoc loc, const AvailabilityInfo &availability,
                             APIAccess access, AccessControl accessControl,
                             DocComment docComment,
                             DeclarationFragments declarationFragments,
                             DeclarationFragments subHeading, const Decl *decl)
      : APIRecord({name, StringRef{}, usr, loc, decl, availability, linkage,
                   APIFlags::None, access, docComment, declarationFragments,
                   subHeading}),
        accessControl(accessControl) {}

  static ObjCInstanceVariableRecord *
  create(llvm::BumpPtrAllocator &allocator, StringRef name, StringRef usr,
         APILinkage linkage, APILoc loc, const AvailabilityInfo &availability,
         APIAccess access, AccessControl accessControl, DocComment docComment,
         DeclarationFragments declarationFragments,
         DeclarationFragments subHeading, const Decl *decl);
};

struct ObjCMethodRecord : APIRecord {
  FunctionSignature signature;
  bool isInstanceMethod;
  bool isOptional;
  bool isDynamic;

  ObjCMethodRecord(StringRef name, StringRef usr, APILoc loc,
                   const AvailabilityInfo &availability, APIAccess access,
                   bool isInstanceMethod, bool isOptional, bool isDynamic,
                   DocComment docComment,
                   DeclarationFragments declarationFragments,
                   DeclarationFragments subHeading, FunctionSignature signature,
                   const Decl *decl)
      : APIRecord({name, StringRef{}, usr, loc, decl, availability,
                   APILinkage::Unknown, APIFlags::None, access, docComment,
                   declarationFragments, subHeading}),
        signature(signature), isInstanceMethod(isInstanceMethod),
        isOptional(isOptional), isDynamic(isDynamic) {}

  static ObjCMethodRecord *
  create(llvm::BumpPtrAllocator &allocator, StringRef name, StringRef usr,
         APILoc loc, const AvailabilityInfo &availability, APIAccess access,
         bool isInstanceMethod, bool isOptional, bool isDynamic,
         DocComment docComment, DeclarationFragments declarationFragments,
         DeclarationFragments subHeading, FunctionSignature signature,
         const Decl *decl);
};

struct ObjCContainerRecord : APIRecord {
  std::vector<ObjCMethodRecord *> methods;
  std::vector<ObjCPropertyRecord *> properties;
  std::vector<ObjCInstanceVariableRecord *> ivars;
  std::vector<SymbolInfo> protocols;

  ObjCContainerRecord(StringRef name, StringRef declName, StringRef usr,
                      APILinkage linkage, APILoc loc,
                      const AvailabilityInfo &availability, APIAccess access,
                      DocComment docComment,
                      DeclarationFragments declarationFragments,
                      DeclarationFragments subHeading, const Decl *decl)
      : APIRecord({name, declName, usr, loc, decl, availability, linkage,
                   APIFlags::None, access, docComment, declarationFragments,
                   subHeading}) {}
};

struct ObjCCategoryRecord : ObjCContainerRecord {
  SymbolInfo interface;

  ObjCCategoryRecord(SymbolInfo interface, StringRef name, StringRef usr,
                     APILoc loc, const AvailabilityInfo &availability,
                     APIAccess access, DocComment docComment,
                     DeclarationFragments declarationFragments,
                     DeclarationFragments subHeading, const Decl *decl)
      : ObjCContainerRecord(name, StringRef{}, usr, APILinkage::Unknown, loc,
                            availability, access, docComment,
                            declarationFragments, subHeading, decl),
        interface(interface) {}

  static ObjCCategoryRecord *
  create(llvm::BumpPtrAllocator &allocator, SymbolInfo interface,
         StringRef name, StringRef usr, APILoc loc,
         const AvailabilityInfo &availability, APIAccess access,
         DocComment docComment, DeclarationFragments declarationFragments,
         DeclarationFragments subHeading, const Decl *decl);
};

struct ObjCProtocolRecord : ObjCContainerRecord {
  ObjCProtocolRecord(StringRef name, StringRef usr, APILoc loc,
                     const AvailabilityInfo &availability, APIAccess access,
                     DocComment docComment,
                     DeclarationFragments declarationFragments,
                     DeclarationFragments subHeading, const Decl *decl)
      : ObjCContainerRecord(name, StringRef{}, usr, APILinkage::Unknown, loc,
                            availability, access, docComment,
                            declarationFragments, subHeading, decl) {}

  static ObjCProtocolRecord *
  create(llvm::BumpPtrAllocator &allocator, StringRef name, StringRef usr,
         APILoc loc, const AvailabilityInfo &availability, APIAccess access,
         DocComment docComment, DeclarationFragments declarationFragments,
         DeclarationFragments subHeading, const Decl *decl);
};

struct ObjCInterfaceRecord : ObjCContainerRecord {
  std::vector<const ObjCCategoryRecord *> categories;
  SymbolInfo superClass;
  bool hasExceptionAttribute = false;

  ObjCInterfaceRecord(StringRef name, StringRef declName, StringRef usr,
                      APILinkage linkage, APILoc loc,
                      const AvailabilityInfo &availability, APIAccess access,
                      SymbolInfo superClass, DocComment docComment,
                      DeclarationFragments declarationFragments,
                      DeclarationFragments subHeading, const Decl *decl)
      : ObjCContainerRecord(name, declName, usr, linkage, loc, availability,
                            access, docComment, declarationFragments,
                            subHeading, decl),
        superClass(superClass) {}

  static ObjCInterfaceRecord *
  create(llvm::BumpPtrAllocator &allocator, StringRef name, StringRef declName,
         StringRef usr, APILinkage linkage, APILoc loc,
         const AvailabilityInfo &availability, APIAccess access,
         SymbolInfo superClass, DocComment docComment,
         DeclarationFragments declarationFragments,
         DeclarationFragments subHeading, const Decl *decl);
};

struct TypedefRecord : APIRecord {
  SymbolInfo underlyingType;

  TypedefRecord(StringRef name, StringRef usr, APILoc loc,
                const AvailabilityInfo &availability, APIAccess access,
                SymbolInfo underlyingType, DocComment docComment,
                DeclarationFragments declarationFragments,
                DeclarationFragments subHeading, const Decl *decl)
      : APIRecord({name, StringRef{}, usr, loc, decl, availability,
                   APILinkage::Unknown, APIFlags::None, access, docComment,
                   declarationFragments, subHeading}),
        underlyingType(underlyingType) {}

  static TypedefRecord *
  create(llvm::BumpPtrAllocator &allocator, StringRef name, StringRef usr,
         APILoc loc, const AvailabilityInfo &availability, APIAccess access,
         SymbolInfo underlyingType, DocComment docComment,
         DeclarationFragments declarationFragments,
         DeclarationFragments subHeading, const Decl *decl);
};

class APIVisitor;
class APIMutator;

struct BinaryInfo {
  FileType fileType = FileType::Invalid;
  PackedVersion currentVersion;
  PackedVersion compatibilityVersion;
  uint8_t swiftABIVersion = 0;
  bool isTwoLevelNamespace = false;
  bool isAppExtensionSafe = false;
  StringRef parentUmbrella;
  std::vector<StringRef> allowableClients;
  std::vector<StringRef> reexportedLibraries;
  StringRef installName;
  StringRef uuid;
};

// Order of the BinaryInfo.
inline bool operator<(const BinaryInfo &lhs, const BinaryInfo &rhs) {
  // Invalid ones goes to the end. Otherwise, first sort by file type.
  if (lhs.fileType != rhs.fileType) {
    if (lhs.fileType == FileType::Invalid)
      return false;
    if (rhs.fileType == FileType::Invalid)
      return true;

    return lhs.fileType < rhs.fileType;
  }

  // Two level names space goes first.
  if (lhs.isTwoLevelNamespace != rhs.isTwoLevelNamespace)
    return lhs.isTwoLevelNamespace;

  // Empty paths goes in the end.
  bool lhsPathEmpty = lhs.installName.empty();
  bool rhsPathEmpty = rhs.installName.empty();
  if (lhsPathEmpty != rhsPathEmpty)
    return rhsPathEmpty;

  // RelativePath goes afterwards.
  bool lhsRelativePath = lhs.installName.startswith("@");
  bool rhsRelativePath = rhs.installName.startswith("@");
  if (lhsRelativePath != rhsRelativePath)
    return rhsRelativePath;

  // Public path goes first.
  bool lhsPublic = isPublicDylib(lhs.installName);
  bool rhsPublic = isPublicDylib(rhs.installName);
  if (lhsPublic != rhsPublic)
    return lhsPublic;

  // Check public location in SDK.
  bool lhsPublicLocation = isWithinPublicLocation(lhs.installName);
  bool rhsPublicLocation = isWithinPublicLocation(rhs.installName);
  if (lhsPublicLocation != rhsPublicLocation)
    return lhsPublicLocation;

  // Last sort by installName.
  return lhs.installName < rhs.installName;
}

// Compare only the bits that differentiage two binaries.
inline bool operator==(const BinaryInfo &lhs, const BinaryInfo &rhs) {
  return std::tie(lhs.fileType, lhs.installName, lhs.isTwoLevelNamespace) ==
         std::tie(rhs.fileType, rhs.installName, rhs.isTwoLevelNamespace);
}

inline bool operator!=(const BinaryInfo &lhs, const BinaryInfo &rhs) {
  return !(lhs == rhs);
}

class API {
public:
  API(const llvm::Triple &triple) : target(triple) {}
  const llvm::Triple &getTarget() const { return target; }

  StringRef getProjectName() const { return projectName; }
  void setProjectName(StringRef project) { projectName = copyString(project); }

  static bool updateAPIAccess(APIRecord *record, APIAccess access);
  static bool updateAPILinkage(APIRecord *record, APILinkage linkage);

  MacroDefinitionRecord *
  addMacroDefinition(StringRef name, StringRef usr, APILoc loc,
                     APIAccess access,
                     DeclarationFragments declarationFragments);

  EnumRecord *addEnum(StringRef name, StringRef declName, StringRef usr,
                      APILoc loc, const AvailabilityInfo &availability,
                      APIAccess access, DocComment docComment,
                      DeclarationFragments declarationFragments,
                      DeclarationFragments subHeading, const Decl *decl);
  EnumConstantRecord *addEnumConstant(
      EnumRecord *record, StringRef name, StringRef declName, StringRef usr,
      APILoc loc, const AvailabilityInfo &availability, APIAccess access,
      DocComment docComment, DeclarationFragments declarationFragments,
      DeclarationFragments subHeading, const Decl *decl);
  GlobalRecord *addGlobal(StringRef name, StringRef declName, StringRef usr,
                          APILoc loc, const AvailabilityInfo &availability,
                          APIAccess access, DocComment docComment,
                          DeclarationFragments declarationFragments,
                          DeclarationFragments subHeading,
                          FunctionSignature functionSignature, const Decl *decl,
                          GVKind kind = GVKind::Unknown,
                          APILinkage linkage = APILinkage::Unknown,
                          bool isWeakDefined = false,
                          bool isThreadLocal = false);
  GlobalRecord *addGlobal(StringRef name, StringRef declName, StringRef usr,
                          APIFlags flags, APILoc loc,
                          const AvailabilityInfo &availability,
                          APIAccess access, DocComment docComment,
                          DeclarationFragments declarationFragments,
                          DeclarationFragments subHeading,
                          FunctionSignature functionSignature, const Decl *decl,
                          GVKind kind = GVKind::Unknown,
                          APILinkage linkage = APILinkage::Unknown);
  GlobalRecord *
  addGlobalVariable(StringRef name, StringRef declName, StringRef usr,
                    APILoc loc, const AvailabilityInfo &availability,
                    APIAccess access, DocComment docComment,
                    DeclarationFragments declarationFragments,
                    DeclarationFragments subHeading, const Decl *decl,
                    APILinkage linkage = APILinkage::Unknown,
                    bool isWeakDefined = false, bool isThreadLocal = false);
  GlobalRecord *addFunction(StringRef name, StringRef declName, StringRef usr,
                            APILoc loc, const AvailabilityInfo &availability,
                            APIAccess access, DocComment docComment,
                            DeclarationFragments declarationFragments,
                            DeclarationFragments subHeading,
                            FunctionSignature functionSignature,
                            const Decl *decl,
                            APILinkage linkage = APILinkage::Unknown,
                            bool isWeakDefined = false);
  ObjCInterfaceRecord *
  addObjCInterface(StringRef name, StringRef declName, StringRef usr,
                   APILoc loc, const AvailabilityInfo &availability,
                   APIAccess access, APILinkage linkage, SymbolInfo superClass,
                   DocComment docComment,
                   DeclarationFragments declarationFragments,
                   DeclarationFragments subHeading, const Decl *decl);
  ObjCCategoryRecord *addObjCCategory(SymbolInfo interface, StringRef name,
                                      StringRef usr, APILoc loc,
                                      const AvailabilityInfo &availability,
                                      APIAccess access, DocComment docComment,
                                      DeclarationFragments declarationFragments,
                                      DeclarationFragments subHeading,
                                      const Decl *decl);
  ObjCProtocolRecord *addObjCProtocol(StringRef name, StringRef usr, APILoc loc,
                                      const AvailabilityInfo &availability,
                                      APIAccess access, DocComment docComment,
                                      DeclarationFragments declarationFragments,
                                      DeclarationFragments subHeading,
                                      const Decl *decl);
  void addObjCProtocol(ObjCContainerRecord *record, SymbolInfo protocol);
  ObjCMethodRecord *
  addObjCMethod(ObjCContainerRecord *record, StringRef name, StringRef usr,
                APILoc loc, const AvailabilityInfo &availability,
                APIAccess access, bool isInstanceMethod, bool isOptional,
                bool isDynamic, DocComment docComment,
                DeclarationFragments declarationFragments,
                DeclarationFragments subHeading, FunctionSignature signature,
                const Decl *decl);
  ObjCPropertyRecord *
  addObjCProperty(ObjCContainerRecord *record, StringRef name, StringRef usr,
                  StringRef getterName, StringRef setterName, APILoc loc,
                  const AvailabilityInfo &availability, APIAccess access,
                  ObjCPropertyRecord::AttributeKind attributes, bool isOptional,
                  DocComment docComment,
                  DeclarationFragments declarationFragments,
                  DeclarationFragments subHeading, const Decl *decl);
  ObjCInstanceVariableRecord *addObjCInstanceVariable(
      ObjCContainerRecord *record, StringRef name, StringRef usr, APILoc loc,
      const AvailabilityInfo &availability, APIAccess access,
      ObjCInstanceVariableRecord::AccessControl accessControl,
      APILinkage linkage, DocComment docComment,
      DeclarationFragments declarationFragments,
      DeclarationFragments subHeading, const Decl *decl);
  StructRecord *addStruct(StringRef name, StringRef usr, APILoc loc,
                          const AvailabilityInfo &availability,
                          APIAccess access, DocComment docComment,
                          DeclarationFragments declarationFragments,
                          DeclarationFragments subHeading, const Decl *decl);
  StructFieldRecord *addStructField(
      StructRecord *record, StringRef name, StringRef declName, StringRef usr,
      APILoc loc, const AvailabilityInfo &availability, APIAccess access,
      DocComment docComment, DeclarationFragments declarationFragments,
      DeclarationFragments subHeading, const Decl *decl);

  TypedefRecord *addTypeDef(StringRef name, StringRef usr, APILoc loc,
                            const AvailabilityInfo &availability,
                            APIAccess access, SymbolInfo underlyingType,
                            DocComment docComment,
                            DeclarationFragments declarationFragments,
                            DeclarationFragments subHeading, const Decl *decl);

  void addPotentiallyDefinedSelector(StringRef name) {
    potentiallyDefinedSelectors.insert(name);
  }

  llvm::StringSet<> &getPotentiallyDefinedSelectors() {
    return potentiallyDefinedSelectors;
  }

  const llvm::StringSet<> &getPotentiallyDefinedSelectors() const {
    return potentiallyDefinedSelectors;
  }

  void visit(APIMutator &visitor);
  void visit(APIVisitor &visitor) const;

  const GlobalRecord *findGlobalVariable(StringRef) const;
  const GlobalRecord *findFunction(StringRef) const;
  const TypedefRecord *findTypeDef(StringRef) const;
  const EnumRecord *findEnum(StringRef name) const;
  const ObjCInterfaceRecord *findObjCInterface(StringRef) const;
  const ObjCProtocolRecord *findObjCProtocol(StringRef) const;
  const ObjCCategoryRecord *findObjCCategory(StringRef, StringRef) const;
  const StructRecord *findStruct(StringRef) const;

  bool hasBinaryInfo() const { return binaryInfo; }
  BinaryInfo &getBinaryInfo();
  const BinaryInfo &getBinaryInfo() const {
    assert(hasBinaryInfo() && "must have binary info");
    return *binaryInfo;
  }

  bool operator<(const API &other) const;

  StringRef copyString(StringRef string);
  static StringRef copyStringInto(StringRef, llvm::BumpPtrAllocator &);

  bool isEmpty() const {
    return !hasBinaryInfo() && globals.empty() && enums.empty() &&
           interfaces.empty() && categories.empty() && protocols.empty() &&
           typeDefs.empty() && potentiallyDefinedSelectors.empty();
  }

private:
  using MacroDefinitionRecordMap =
      llvm::MapVector<StringRef, MacroDefinitionRecord *>;
  using GlobalRecordMap = llvm::MapVector<StringRef, GlobalRecord *>;
  using EnumRecordMap = llvm::MapVector<StringRef, EnumRecord *>;
  using ObjCInterfaceRecordMap =
      llvm::MapVector<StringRef, ObjCInterfaceRecord *>;
  using ObjCCategoryRecordMap =
      llvm::MapVector<std::pair<StringRef, StringRef>, ObjCCategoryRecord *>;
  using ObjCProtocolRecordMap =
      llvm::MapVector<StringRef, ObjCProtocolRecord *>;
  using TypedefMap = llvm::MapVector<StringRef, TypedefRecord *>;
  using StructRecordMap = llvm::MapVector<StringRef, StructRecord *>;

  llvm::BumpPtrAllocator allocator;

  const llvm::Triple target;

  MacroDefinitionRecordMap macros;
  GlobalRecordMap globals;
  EnumRecordMap enums;
  ObjCInterfaceRecordMap interfaces;
  ObjCCategoryRecordMap categories;
  ObjCProtocolRecordMap protocols;
  TypedefMap typeDefs;
  StructRecordMap structs;
  llvm::StringSet<> potentiallyDefinedSelectors;

  StringRef projectName;
  BinaryInfo *binaryInfo = nullptr;

  friend class APIVerifier;
};

TAPI_NAMESPACE_INTERNAL_END

#endif // TAPI_CORE_API_H
