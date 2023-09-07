//===- lib/Frontend/APIVisitor.cpp - TAPI API Visitor -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Implements the TAPI API Visitor.
///
//===----------------------------------------------------------------------===//

#include "APIVisitor.h"
#include "DeclarationFragmentsBuilder.h"
#include "tapi/Defines.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/CommentCommandTraits.h"
#include "clang/AST/CommentLexer.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/AST/RawCommentList.h"
#include "clang/AST/VTableBuilder.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Index/USRGeneration.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace TAPI_INTERNAL;

namespace {
enum class LinkageType {
  ExternalLinkage,
  LinkOnceODRLinkage,
  WeakODRLinkage,
  PrivateLinkage,
};
}

TAPI_NAMESPACE_INTERNAL_BEGIN

DocComment::DocComment(const clang::RawComment *rawComment,
                       const clang::SourceManager &sourceManager,
                       clang::DiagnosticsEngine &diags) {
  // Skip if there is no doc comment attached to the decl node
  if (!rawComment)
    return;

  StringRef rawText = rawComment->getRawText(sourceManager);
  if (rawText.empty())
    return;

  // FIXME: The following parsing logic was adapted from
  // clang::RawComment::getFormattedText. The original function cannot be used
  // directly because we want individual source range for each line of the
  // comment text, while the original getFormattedText method only returns a
  // formatted string of the whole comment block.
  // We can reuse the parsing logic and provide different views directly from
  // clang::RawComment once upstreamed.

  BumpPtrAllocator allocator;
  // Default-construct a comment option as we don't parse comment commands
  clang::CommentOptions defaultOptions;
  clang::comments::CommandTraits emptyTraits(allocator, defaultOptions);
  clang::comments::Lexer lexer(allocator, diags, emptyTraits,
                               rawComment->getBeginLoc(), rawText.begin(),
                               rawText.end(), /*ParseCommands=*/false);

  // Record the column number of the first non-whitespace token in the first
  // line. Whitespace up to this column is skipped.
  unsigned indentColumn = 0;

  // Record the line number of the last processed comment line.
  // For block-style comments, an extra newline token will be produced after
  // the end-comment marker, e.g.:
  //   /** This is a multi-line comment block.
  //       The lexer will produce two newline tokens here > */
  // previousLine will record the line number when we previously saw a newline
  // token and recorded a comment line. If we see another newline token on the
  // same line, don't record anything in between.
  unsigned previousLine = 0;

  // Processes one line of the comment and adds the text and the adjusted source
  // range to commentLines. Newline characters are stripped. Returns false when
  // reaching eof and true otherwise.
  auto lexLine = [&](bool isFirstLine) -> bool {
    clang::comments::Token tok;
    // First token of the line is processed separately to handle indentation.
    lexer.lex(tok);
    if (tok.is(clang::comments::tok::eof))
      return false;
    if (tok.is(clang::comments::tok::newline)) {
      APILoc loc = sourceManager.getPresumedLoc(tok.getLocation());
      if (loc.getLine() != previousLine) {
        commentLines.emplace_back("", loc, loc);
        previousLine = loc.getLine();
      }
      return true;
    }

    std::string line;
    StringRef tokText = lexer.getSpelling(tok, sourceManager);
    bool locInvalid = false;
    unsigned tokColumn =
        sourceManager.getSpellingColumnNumber(tok.getLocation(), &locInvalid);
    assert(!locInvalid && "Invalid location");

    // Amount of leading whitespace in tokText.
    size_t whitespaceLen = tokText.find_first_not_of(" \t");
    if (whitespaceLen == StringRef::npos)
      whitespaceLen = tokText.size();
    // Remember the amount of whitespace skipped in the first line to remove
    // indent up to that column in the following lines.
    if (isFirstLine)
      indentColumn = tokColumn + whitespaceLen;

    // Amount of leading whitespace we actually want to skip.
    // For the first line we skip all the whitespace.
    // For the rest of the lines, we skip up to indentColumn.
    unsigned skipLen =
        isFirstLine
            ? whitespaceLen
            : std::min<size_t>(
                  whitespaceLen,
                  std::max<int>(static_cast<int>(indentColumn) - tokColumn, 0));
    StringRef trimmed = tokText.drop_front(skipLen);
    line += trimmed;
    // Get the beginning location of the adjusted comment line.
    APILoc beginLoc = sourceManager.getPresumedLoc(
        tok.getLocation().getLocWithOffset(skipLen));

    // Lex the rest of tokens in this line.
    for (lexer.lex(tok); tok.isNot(clang::comments::tok::eof); lexer.lex(tok)) {
      if (tok.is(clang::comments::tok::newline)) {
        // Get the ending location of the comment line.
        APILoc endLoc = sourceManager.getPresumedLoc(tok.getLocation());
        if (endLoc.getLine() != previousLine) {
          commentLines.emplace_back(line, beginLoc, endLoc);
          previousLine = endLoc.getLine();
        }
        return true;
      }
      line += lexer.getSpelling(tok, sourceManager);
    }
    APILoc endLoc = sourceManager.getPresumedLoc(tok.getLocation());
    commentLines.emplace_back(line, beginLoc, endLoc);
    return false;
  };

  // Process the first line separately to remember indent for the following
  // lines.
  if (!lexLine(/*isFirstLine=*/true))
    return;
  // Process the rest of the lines.
  while (lexLine(/*isFirstLine=*/false))
    ;
  return;
}

TAPI_NAMESPACE_INTERNAL_END

namespace clang {

// \brief Check if the interface itself or any of its super classes have an
// exception attribute.
//
// We need to export an additonal symbol ("OBJC_EHTYPE_$CLASS_NAME") if any of
// the classes have the exception attribute.
static bool hasObjCExceptionAttribute(const ObjCInterfaceDecl *decl) {
  for (; decl != nullptr; decl = decl->getSuperClass())
    if (decl->hasAttr<ObjCExceptionAttr>())
      return true;

  return false;
}

static bool isInlined(const ASTContext &context, const FunctionDecl *func) {
  // Check all redeclarations to find the inline attribute / keyword.
  bool hasInlineAttribute = false;
  for (const auto *decl : func->redecls()) {
    if (decl->isInlined()) {
      hasInlineAttribute = true;
      break;
    }
  }
  if (!hasInlineAttribute)
    return false;

  if ((!context.getLangOpts().CPlusPlus &&
       !context.getTargetInfo().getCXXABI().isMicrosoft() &&
       !func->hasAttr<DLLExportAttr>()) ||
      func->hasAttr<GNUInlineAttr>()) {
    if (func->doesThisDeclarationHaveABody() &&
        func->isInlineDefinitionExternallyVisible())
      return false;
  }

  return true;
}

// \brief Check if the NamedDecl is exported or not.
//
// Exported NamedDecl needs to have externally visibiliy linkage and
// default visibility from LinkageComputer.
static bool isExported(const NamedDecl *decl) {
  auto li = decl->getLinkageAndVisibility();
  return isExternallyVisible(li.getLinkage()) &&
         (li.getVisibility() == DefaultVisibility);
}

static bool hasVTable(ASTContext &context, const CXXRecordDecl *decl) {
  // Check if we need need to emit the vtable symbol. Only dynamic classes need
  // vtables.
  if (!decl->hasDefinition() || !decl->isDynamicClass())
    return false;

  assert(decl->isExternallyVisible() && "should be externally visible");
  assert(decl->isCompleteDefinition() && "only work on complete definitions");

  const auto *keyFunction = context.getCurrentKeyFunction(decl);
  // If this class has a key function, then we have a vtable (might be internal
  // only).
  if (keyFunction) {
    switch (keyFunction->getTemplateSpecializationKind()) {
    case TSK_Undeclared:
    case TSK_ExplicitSpecialization:
    case TSK_ImplicitInstantiation:
    case TSK_ExplicitInstantiationDefinition:
      return true;
    case TSK_ExplicitInstantiationDeclaration:
      llvm_unreachable("Should not have been asked to emit this");
    }
  } else if (decl->isAbstract())
    // If the class is abstract and it doesn't have a key function, it is a
    // 'pure' virtual class. It doesn't need a VTable.
    return false;

  switch (decl->getTemplateSpecializationKind()) {
  case TSK_Undeclared:
  case TSK_ExplicitSpecialization:
  case TSK_ImplicitInstantiation:
    return false;

  case TSK_ExplicitInstantiationDeclaration:
  case TSK_ExplicitInstantiationDefinition:
    return true;
  }

  llvm_unreachable("Invalid TemplateSpecializationKind!");
}

static LinkageType getVTableLinkage(ASTContext &context,
                                    const CXXRecordDecl *decl) {
  assert((decl->hasDefinition() && decl->isDynamicClass()) && "no vtable");
  assert(decl->isExternallyVisible() && "should be externally visible");

  if (decl->getVisibility() == HiddenVisibility)
    return LinkageType::PrivateLinkage;

  const CXXMethodDecl *keyFunction = context.getCurrentKeyFunction(decl);
  if (keyFunction) {
    // If this class has a key function, use that to determine the
    // linkage of the vtable.
    switch (keyFunction->getTemplateSpecializationKind()) {
    case TSK_Undeclared:
    case TSK_ExplicitSpecialization:
      if (isInlined(context, keyFunction))
        return LinkageType::LinkOnceODRLinkage;
      return LinkageType::ExternalLinkage;
    case TSK_ImplicitInstantiation:
      llvm_unreachable("no external vtable for implicit instantiation");
    case TSK_ExplicitInstantiationDefinition:
      return LinkageType::WeakODRLinkage;
    case TSK_ExplicitInstantiationDeclaration:
      llvm_unreachable("Should not have been asked to emit this");
    }
  }

  switch (decl->getTemplateSpecializationKind()) {
  case TSK_Undeclared:
  case TSK_ExplicitSpecialization:
  case TSK_ImplicitInstantiation:
    return LinkageType::LinkOnceODRLinkage;
  case TSK_ExplicitInstantiationDeclaration:
  case TSK_ExplicitInstantiationDefinition:
    return LinkageType::WeakODRLinkage;
  }

  llvm_unreachable("Invalid TemplateSpecializationKind!");
}

static bool isRTTIWeakDef(ASTContext &context, const CXXRecordDecl *decl) {
  if (decl->hasAttr<WeakAttr>())
    return true;

  if (decl->isAbstract() && context.getCurrentKeyFunction(decl) == nullptr)
    return true;

  if (decl->isDynamicClass())
    return getVTableLinkage(context, decl) != LinkageType::ExternalLinkage;

  return false;
}

static bool hasRTTI(ASTContext &context, const CXXRecordDecl *decl) {
  if (!context.getLangOpts().RTTI)
    return false;

  if (!decl->hasDefinition())
    return false;

  if (!decl->isDynamicClass())
    return false;

  // Don't emit weak-def RTTI information. We cannot reliably determine if the
  // final binary will have those weak defined RTTI symbols. This depends on the
  // optimization level and if the class has been instantiated and used.
  //
  // Luckily the static linker doesn't need those weak defined RTTI symbols for
  // linking. They are only needed by the runtime linker. That means we can
  // safely drop all of them.
  if (isRTTIWeakDef(context, decl))
    return false;

  return true;
}

static Optional<std::pair<APIAccess, PresumedLoc>>
getFileAttributesForLoc(FrontendContext &context, SourceLocation loc) {
  // If the loc refers to a macro expansion we need to first get the file
  // location of the expansion.
  auto fileLoc = context.sourceMgr->getFileLoc(loc);
  FileID id = context.sourceMgr->getFileID(fileLoc);
  if (id.isInvalid())
    return None;

  const auto *file = context.sourceMgr->getFileEntryForID(id);
  if (!file)
    return None;

  auto presumedLoc = context.sourceMgr->getPresumedLoc(loc);

  auto header = context.findAndRecordFile(file);
  if (!header.hasValue())
    return None;

  APIAccess access;
  switch (header.getValue()) {
  case HeaderType::Public:
    access = APIAccess::Public;
    break;
  case HeaderType::Private:
    access = APIAccess::Private;
    break;
  case HeaderType::Project:
    access = APIAccess::Project;
    break;
  }

  return std::make_pair(access, presumedLoc);
}

APIVisitor::APIVisitor(FrontendContext &frontend)
    : frontend(frontend), context(frontend.compiler->getASTContext()),
      sourceManager(context.getSourceManager()),
      mc(clang::ItaniumMangleContext::create(context,
                                             context.getDiagnostics())),
      dataLayout(context.getTargetInfo().getDataLayoutString()) {}

void APIVisitor::HandleTranslationUnit(ASTContext &context) {
  if (context.getDiagnostics().hasErrorOccurred())
    return;

  auto *decl = context.getTranslationUnitDecl();
  TraverseDecl(decl);
}

Optional<std::pair<APIAccess, PresumedLoc>>
APIVisitor::getFileAttributesForDecl(const NamedDecl *decl) const {
  auto loc = decl->getLocation();
  if (loc.isInvalid())
    return None;

  return getFileAttributesForLoc(frontend, loc);
}

std::string APIVisitor::getMangledName(const NamedDecl *decl) const {
  SmallString<256> name;
  if (mc->shouldMangleDeclName(decl)) {
    raw_svector_ostream nameStream(name);
    mc->mangleName(decl, nameStream);
  } else
    name += decl->getNameAsString();

  return getBackendMangledName(name);
}

std::string APIVisitor::getBackendMangledName(Twine name) const {
  SmallString<256> finalName;
  Mangler::getNameWithPrefix(finalName, name, DataLayout(dataLayout));
  return finalName.str().str();
}

std::string
APIVisitor::getMangledCXXVTableName(const CXXRecordDecl *decl) const {
  SmallString<256> name;
  raw_svector_ostream nameStream(name);
  mc->mangleCXXVTable(decl, nameStream);

  return getBackendMangledName(name);
}

std::string APIVisitor::getMangledCXXRTTI(const CXXRecordDecl *decl) const {
  SmallString<256> name;
  raw_svector_ostream nameStream(name);
  mc->mangleCXXRTTI(QualType(decl->getTypeForDecl(), 0), nameStream);

  return getBackendMangledName(name);
}

std::string APIVisitor::getMangledCXXRTTIName(const CXXRecordDecl *decl) const {
  SmallString<256> name;
  raw_svector_ostream nameStream(name);
  mc->mangleCXXRTTIName(QualType(decl->getTypeForDecl(), 0), nameStream);

  return getBackendMangledName(name);
}

std::string APIVisitor::getMangledCXXThunk(const GlobalDecl &decl,
                                           const ThunkInfo &thunk,
                                           bool elideOverrideInfo) const {
  SmallString<256> name;
  raw_svector_ostream nameStream(name);
  const auto *method = cast<CXXMethodDecl>(decl.getDecl());
  if (const auto *dtor = dyn_cast<CXXDestructorDecl>(method))
    mc->mangleCXXDtorThunk(dtor, decl.getDtorType(), thunk.This, nameStream);
  else
    mc->mangleThunk(method, thunk, nameStream);

  return getBackendMangledName(name);
}

std::string APIVisitor::getMangledCtorDtor(const CXXMethodDecl *decl,
                                           int type) const {
  SmallString<256> name;
  raw_svector_ostream nameStream(name);
  GlobalDecl gd;
  if (const auto *ctor = dyn_cast<CXXConstructorDecl>(decl))
    gd = GlobalDecl(ctor, CXXCtorType(type));
  else {
    const auto *dtor = cast<CXXDestructorDecl>(decl);
    gd = GlobalDecl(dtor, CXXDtorType(type));
  }
  mc->mangleName(gd, nameStream);
  return getBackendMangledName(name);
}

AvailabilityInfo APIVisitor::getAvailabilityInfo(const Decl *decl) const {
  auto platformName = context.getTargetInfo().getPlatformName();

  AvailabilityInfo availability;
  for (const auto *decl : decl->redecls()) {
    for (const auto *A : decl->specific_attrs<AvailabilityAttr>()) {
      if (A->getPlatform()->getName() != platformName)
        continue;

      availability = AvailabilityInfo(A->getIntroduced(), A->getDeprecated(),
                                      A->getObsoleted(), A->getUnavailable(),
                                      false, isAvailabilitySPI(A->getLoc()));
      break;
    }

    if (const auto *attr = decl->getAttr<UnavailableAttr>())
      if (!attr->isImplicit())
        availability._unavailable = true;

    if (const auto *attr = decl->getAttr<DeprecatedAttr>())
      if (!attr->isImplicit())
        availability._unconditionallyDeprecated = true;
  }

  // Return default availability.
  return availability;
}

bool APIVisitor::isAvailabilitySPI(SourceLocation loc) const {
  // Check to see if spelling location is in AvailabilityInternalPrivate.h.
  auto &sm = context.getSourceManager();
  auto spellingLoc = sm.getSpellingLoc(loc);
  auto *file = sm.getFileEntryForID(sm.getFileID(spellingLoc));
  if (file && file->getName().endswith("AvailabilityInternalPrivate.h")) 
    return true;
  // If not, search up the macro expansion see if the previous expansion
  // satisfy the requirement.
  if (!loc.isMacroID())
    return false;
  auto &expansion = sm.getSLocEntry(sm.getFileID(loc)).getExpansion();
  return isAvailabilitySPI(expansion.getExpansionLocStart());
}

StringRef APIVisitor::getTypedefName(const TagDecl *decl) const {
  if (const auto *typedefDecl = decl->getTypedefNameForAnonDecl())
    return typedefDecl->getName();

  return {};
}

/// Collect all global variables.
bool APIVisitor::VisitVarDecl(const VarDecl *decl) {
  // Skip function parameters.
  if (isa<ParmVarDecl>(decl))
    return true;

  // Skip variables in records. They are already handled in VisitCXXRecordDecl.
  if (decl->getDeclContext()->isRecord())
    return true;

  // Skip VarDecl inside function or method.
  if (!decl->isDefinedOutsideFunctionOrMethod())
    return true;

  // If this is a template but not specialization or instantiation, skip.
  if (decl->getASTContext().getTemplateOrSpecializationInfo(decl) &&
      decl->getTemplateSpecializationKind() == TSK_Undeclared)
    return true;

  auto attributes = getFileAttributesForDecl(decl);
  if (!attributes)
    return true;
  APIAccess access;
  PresumedLoc loc;
  std::tie(access, loc) = attributes.getValue();
  auto name = getMangledName(decl);
  auto declName = decl->getName();
  auto avail = getAvailabilityInfo(decl);
  bool isWeakDef = decl->hasAttr<WeakAttr>();
  bool isThreadLocal = decl->getTLSKind() != VarDecl::TLS_None;
  auto linkage = isExported(decl) ? APILinkage::Exported : APILinkage::Internal;
  SmallString<128> usr;
  clang::index::generateUSRForDecl(decl, usr);
  DocComment docComment(context.getRawCommentForDeclNoCache(decl),
                        context.getSourceManager(), context.getDiagnostics());

  frontend.api.addGlobalVariable(
      name, declName, usr, loc, avail, access, docComment,
      DeclarationFragmentsBuilder::getFragmentsForVar(decl),
      DeclarationFragmentsBuilder::getSubHeading(decl), decl, linkage,
      isWeakDef, isThreadLocal);

  return true;
}

bool APIVisitor::VisitFunctionDecl(const FunctionDecl *decl) {
  if (auto method = dyn_cast<CXXMethodDecl>(decl)) {
    // Skip member function in class templates.
    if (method->getParent()->getDescribedClassTemplate() != nullptr)
      return true;

    // Skip methods in records. They are already handled in VisitCXXRecordDecl.
    for (auto p : context.getParents(*method)) {
      if (p.get<CXXRecordDecl>())
        return true;
    }

    // ConstructorDecl and DestructorDecl are handled in CXXRecord.
    if (isa<CXXConstructorDecl>(method) || isa<CXXDestructorDecl>(method))
      return true;
  }

  // Skip templated functions.
  switch (decl->getTemplatedKind()) {
  case FunctionDecl::TK_NonTemplate:
    break;
  case FunctionDecl::TK_MemberSpecialization:
  case FunctionDecl::TK_FunctionTemplateSpecialization:
    if (auto *templateInfo = decl->getTemplateSpecializationInfo()) {
      if (!templateInfo->isExplicitInstantiationOrSpecialization())
        return true;
    }
    break;
  case FunctionDecl::TK_FunctionTemplate:
  case FunctionDecl::TK_DependentFunctionTemplateSpecialization:
    return true;
  }

  auto attributes = getFileAttributesForDecl(decl);
  if (!attributes)
    return true;
  APIAccess access;
  PresumedLoc loc;
  auto name = getMangledName(decl);
  auto declName = decl->getName();
  std::tie(access, loc) = attributes.getValue();
  auto avail = getAvailabilityInfo(decl);
  bool isExplicitInstantiation = decl->getTemplateSpecializationKind() ==
                                 TSK_ExplicitInstantiationDeclaration;
  bool isWeakDef = isExplicitInstantiation || decl->hasAttr<WeakAttr>();
  APILinkage linkage = isInlined(context, decl) || !isExported(decl)
                           ? APILinkage::Internal
                           : APILinkage::Exported;
  SmallString<128> usr;
  clang::index::generateUSRForDecl(decl, usr);
  DocComment docComment(context.getRawCommentForDeclNoCache(decl),
                        context.getSourceManager(), context.getDiagnostics());

  frontend.api.addFunction(
      name, declName, usr, loc, avail, access, docComment,
      DeclarationFragmentsBuilder::getFragmentsForFunction(decl),
      DeclarationFragmentsBuilder::getSubHeading(decl),
      DeclarationFragmentsBuilder::getFunctionSignature(decl), decl, linkage,
      isWeakDef);

  return true;
}

bool APIVisitor::VisitEnumDecl(const EnumDecl *decl) {
  if (!decl->isComplete())
    return true;

  // Skip forward declaration.
  if (!decl->isThisDeclarationADefinition())
    return true;

  auto attributes = getFileAttributesForDecl(decl);
  if (!attributes)
    return true;
  APIAccess access;
  PresumedLoc loc;
  std::tie(access, loc) = attributes.getValue();
  auto avail = getAvailabilityInfo(decl);
  auto name = decl->getQualifiedNameAsString();
  auto declName = decl->getName();
  if (declName.empty()) {
    declName = getTypedefName(decl);
  }
  SmallString<128> usr;
  clang::index::generateUSRForDecl(decl, usr);
  DocComment docComment(context.getRawCommentForDeclNoCache(decl),
                        context.getSourceManager(), context.getDiagnostics());

  auto *enumRecord = frontend.api.addEnum(
      name, declName, usr, loc, avail, access, docComment,
      DeclarationFragmentsBuilder::getFragmentsForEnum(decl),
      DeclarationFragmentsBuilder::getSubHeading(decl), decl);

  recordEnumConstants(enumRecord, decl->enumerators());

  return true;
}

void APIVisitor::recordEnumConstants(
    EnumRecord *record, const EnumDecl::enumerator_range constants) {
  for (const auto *enumConstant : constants) {
    auto attributes = getFileAttributesForDecl(enumConstant);
    if (!attributes)
      continue;
    APIAccess access;
    PresumedLoc loc;
    std::tie(access, loc) = attributes.getValue();
    auto avail = getAvailabilityInfo(enumConstant);
    auto name = enumConstant->getQualifiedNameAsString();
    auto declName = enumConstant->getName();
    SmallString<128> usr;
    clang::index::generateUSRForDecl(enumConstant, usr);
    DocComment docComment(context.getRawCommentForDeclNoCache(enumConstant),
                          context.getSourceManager(), context.getDiagnostics());

    frontend.api.addEnumConstant(
        record, name, declName, usr, loc, avail, access, docComment,
        DeclarationFragmentsBuilder::getFragmentsForEnumConstant(enumConstant),
        DeclarationFragmentsBuilder::getSubHeading(enumConstant), enumConstant);
  }
}

/// \brief Visit all Objective-C Interface declarations.
///
/// Every Objective-C class has an interface declaration that lists all the
/// ivars, properties, and methods of the class.
///
bool APIVisitor::VisitObjCInterfaceDecl(const ObjCInterfaceDecl *decl) {
  // Skip forward declaration for classes (@class)
  if (!decl->isThisDeclarationADefinition())
    return true;

  // Get super class.
  StringRef superClassName;
  SmallString<128> superClassUSR;
  StringRef superClassSourceModule;
  if (const auto *superClass = decl->getSuperClass()) {
    superClassName = superClass->getObjCRuntimeNameAsString();
    index::generateUSRForDecl(superClass, superClassUSR);
    superClassSourceModule = getSourceModule(superClass, sourceManager);
  }
  SymbolInfo super(superClassName, superClassUSR, superClassSourceModule);

  auto attributes = getFileAttributesForDecl(decl);
  if (!attributes)
    return true;

  // When the interface is not exported, then there are no linkable symbols
  // exported from the library. The Objective-C metadata for the class and
  // selectors on the other hand are always recorded.
  auto linkage = isExported(decl) ? APILinkage::Exported : APILinkage::Internal;

  // Record the ObjC Class
  auto name = decl->getObjCRuntimeNameAsString();
  auto declName = decl->getName();
  APIAccess access;
  PresumedLoc loc;
  std::tie(access, loc) = attributes.getValue();
  auto avail = getAvailabilityInfo(decl);
  SmallString<128> usr;
  clang::index::generateUSRForDecl(decl, usr);
  DocComment docComment(context.getRawCommentForDeclNoCache(decl),
                        context.getSourceManager(), context.getDiagnostics());

  auto *objcClass = frontend.api.addObjCInterface(
      name, declName, usr, loc, avail, access, linkage, super, docComment,
      DeclarationFragmentsBuilder::getFragmentsForObjCInterface(decl),
      DeclarationFragmentsBuilder::getSubHeading(decl), decl);
  objcClass->hasExceptionAttribute =
      !context.getLangOpts().ObjCRuntime.isFragile() &&
      hasObjCExceptionAttribute(decl);

  // Record all methods (selectors). This doesn't include automatically
  // synthesized property methods.
  recordObjCMethods(objcClass, decl->methods());
  recordObjCProperties(objcClass, decl->properties());
  recordObjCInstanceVariables(objcClass, decl->ivars());
  recordObjCProtocols(objcClass, decl->protocols());

  return true;
}

/// \brief Visit all Objective-C Category/Extension declarations.
///
/// Objective-C classes may have category or extension declarations that list
/// additional ivars, properties, and methods for the class.
///
/// The class that is being extended might come from a different framework and
/// is therefore itself not recorded.
///
bool APIVisitor::VisitObjCCategoryDecl(const ObjCCategoryDecl *decl) {
  auto name = decl->getName();
  auto attributes = getFileAttributesForDecl(decl);
  if (!attributes)
    return true;
  APIAccess access;
  PresumedLoc loc;
  std::tie(access, loc) = attributes.getValue();
  auto avail = getAvailabilityInfo(decl);

  const ObjCInterfaceDecl *interfaceDecl = decl->getClassInterface();
  StringRef interfaceName = interfaceDecl->getName();
  StringRef interfaceSourceModule =
      getSourceModule(interfaceDecl, sourceManager);
  SmallString<128> interfaceUSR;
  clang::index::generateUSRForDecl(interfaceDecl, interfaceUSR);
  SymbolInfo interface(interfaceName, interfaceUSR, interfaceSourceModule);

  SmallString<128> usr;
  clang::index::generateUSRForDecl(decl, usr);
  DocComment docComment(context.getRawCommentForDeclNoCache(decl),
                        context.getSourceManager(), context.getDiagnostics());

  auto *category = frontend.api.addObjCCategory(
      interface, name, usr, loc, avail, access, docComment,
      DeclarationFragmentsBuilder::getFragmentsForObjCCategory(decl),
      DeclarationFragmentsBuilder::getSubHeading(decl), decl);

  // Methods in the CoreDataGeneratedAccessors category are dynamically
  // generated during runtime.
  bool isDynamic = name == "CoreDataGeneratedAccessors";
  recordObjCMethods(category, decl->methods(), isDynamic);
  recordObjCProperties(category, decl->properties());
  recordObjCInstanceVariables(category, decl->ivars());
  recordObjCProtocols(category, decl->protocols());

  return true;
}

/// \brief Visit all Objective-C Protocol declarations.
bool APIVisitor::VisitObjCProtocolDecl(const ObjCProtocolDecl *decl) {
  // Skip forward declaration for protocols (@protocol).
  if (!decl->isThisDeclarationADefinition())
    return true;

  auto name = decl->getName();
  auto attributes = getFileAttributesForDecl(decl);
  if (!attributes)
    return true;
  APIAccess access;
  PresumedLoc loc;
  std::tie(access, loc) = attributes.getValue();
  auto avail = getAvailabilityInfo(decl);
  SmallString<128> usr;
  clang::index::generateUSRForDecl(decl, usr);
  DocComment docComment(context.getRawCommentForDeclNoCache(decl),
                        context.getSourceManager(), context.getDiagnostics());

  auto *protocol = frontend.api.addObjCProtocol(
      name, usr, loc, avail, access, docComment,
      DeclarationFragmentsBuilder::getFragmentsForObjCProtocol(decl),
      DeclarationFragmentsBuilder::getSubHeading(decl), decl);
  recordObjCMethods(protocol, decl->methods());
  recordObjCProperties(protocol, decl->properties());
  recordObjCProtocols(protocol, decl->protocols());

  return true;
}

void APIVisitor::recordObjCMethods(
    ObjCContainerRecord *record, const ObjCContainerDecl::method_range methods,
    bool isDynamic) {
  for (const auto *method : methods) {
    // Don't record selectors for properties.
    if (method->isPropertyAccessor())
      continue;
    auto name = method->getSelector().getAsString();
    auto attributes = getFileAttributesForDecl(method);
    if (!attributes)
      continue;
    APIAccess access;
    PresumedLoc loc;
    std::tie(access, loc) = attributes.getValue();
    auto avail = getAvailabilityInfo(method);
    SmallString<128> usr;
    clang::index::generateUSRForDecl(method, usr);
    DocComment docComment(context.getRawCommentForDeclNoCache(method),
                          context.getSourceManager(), context.getDiagnostics());

    frontend.api.addObjCMethod(
        record, name, usr, loc, avail, access, method->isInstanceMethod(),
        method->isOptional(), isDynamic, docComment,
        DeclarationFragmentsBuilder::getFragmentsForObjCMethod(method),
        DeclarationFragmentsBuilder::getSubHeading(method),
        DeclarationFragmentsBuilder::getSignatureForObjCMethod(method), method);
  }
}

void APIVisitor::recordObjCProperties(
    ObjCContainerRecord *record,
    const ObjCContainerDecl::prop_range properties) {
  for (const auto *property : properties) {
    auto attributes = getFileAttributesForDecl(property);
    if (!attributes)
      continue;
    APIAccess access;
    PresumedLoc loc;
    std::tie(access, loc) = attributes.getValue();
    auto name = property->getName();
    auto getter = property->getGetterName().getAsString();
    auto setter = property->getSetterName().getAsString();
    auto avail = getAvailabilityInfo(property);
    // Get the attributes for property.
    unsigned attr = ObjCPropertyRecord::NoAttr;
    if (property->getPropertyAttributes() &
        ObjCPropertyAttribute::kind_readonly)
      attr |= ObjCPropertyRecord::ReadOnly;
    if (property->getPropertyAttributes() & ObjCPropertyAttribute::kind_class)
      attr |= ObjCPropertyRecord::Class;
    SmallString<128> usr;
    clang::index::generateUSRForDecl(property, usr);
    DocComment docComment(context.getRawCommentForDeclNoCache(property),
                          context.getSourceManager(), context.getDiagnostics());

    frontend.api.addObjCProperty(
        record, name, usr, getter, setter, loc, avail, access,
        (ObjCPropertyRecord::AttributeKind)attr, property->isOptional(),
        docComment,
        DeclarationFragmentsBuilder::getFragmentsForObjCProperty(property),
        DeclarationFragmentsBuilder::getSubHeading(property),
        property);
  }
}

void APIVisitor::recordObjCInstanceVariables(
    ObjCContainerRecord *record,
    const iterator_range<DeclContext::specific_decl_iterator<ObjCIvarDecl>>
        ivars) {
  auto linkage = context.getLangOpts().ObjCRuntime.isFragile()
                     ? APILinkage::Unknown
                     : APILinkage::Exported;
  for (const auto *ivar : ivars) {
    auto attributes = getFileAttributesForDecl(ivar);
    if (!attributes)
      continue;
    APIAccess access;
    PresumedLoc loc;
    auto name = ivar->getName();
    std::tie(access, loc) = attributes.getValue();
    auto avail = getAvailabilityInfo(ivar);
    auto accessControl = ivar->getCanonicalAccessControl();
    SmallString<128> usr;
    clang::index::generateUSRForDecl(ivar, usr);
    DocComment docComment(context.getRawCommentForDeclNoCache(ivar),
                          context.getSourceManager(), context.getDiagnostics());

    frontend.api.addObjCInstanceVariable(
        record, name, usr, loc, avail, access, accessControl, linkage,
        docComment, DeclarationFragmentsBuilder::getFragmentsForField(ivar),
        DeclarationFragmentsBuilder::getSubHeading(ivar), ivar);
  }
}

void APIVisitor::recordObjCProtocols(
    ObjCContainerRecord *container,
    ObjCInterfaceDecl::protocol_range protocols) {
  for (const auto *protocol : protocols) {
    SmallString<128> usr;
    clang::index::generateUSRForDecl(protocol, usr);
    frontend.api.addObjCProtocol(
        container,
        {protocol->getName(), usr, getSourceModule(protocol, sourceManager)});
  }
}

void APIVisitor::emitVTableSymbols(const CXXRecordDecl *decl, PresumedLoc loc,
                                   AvailabilityInfo avail, APIAccess access,
                                   bool emittedVTable) {
  SmallString<128> usr;
  clang::index::generateUSRForDecl(decl, usr);
  DocComment docComment(context.getRawCommentForDeclNoCache(decl),
                        context.getSourceManager(), context.getDiagnostics());

  // FIXME: generate declaration fragments as necessary when we support C++

  if (hasVTable(context, decl)) {
    emittedVTable = true;
    auto vtableLinkage = getVTableLinkage(context, decl);
    if (vtableLinkage == LinkageType::ExternalLinkage ||
        vtableLinkage == LinkageType::WeakODRLinkage) {
      auto name = getMangledCXXVTableName(decl);
      bool isWeakDef = vtableLinkage == LinkageType::WeakODRLinkage;
      frontend.api.addGlobalVariable(name, StringRef{}, usr, loc, avail, access,
                                     docComment, {}, {}, nullptr,
                                     APILinkage::Exported, isWeakDef);

      if (!decl->getDescribedClassTemplate() && !decl->isInvalidDecl()) {
        auto vtable = context.getVTableContext();
        auto addThunk = [&](GlobalDecl decl) {
          auto *thunks = vtable->getThunkInfo(decl);
          if (!thunks)
            return;

          for (auto &thunk : *thunks) {
            auto name =
                getMangledCXXThunk(decl, thunk, /*elideOverrideInfo*/ true);
            SmallString<128> usr;
            clang::index::generateUSRForDecl(decl.getDecl(), usr);
            DocComment docComment(
                context.getRawCommentForDeclNoCache(decl.getDecl()),
                context.getSourceManager(), context.getDiagnostics());
            frontend.api.addFunction(name, StringRef{}, usr, loc, avail, access,
                                     docComment, {}, {}, {}, nullptr,
                                     APILinkage::Exported);
          }
        };

        for (auto *method : decl->methods()) {
          if (isa<CXXConstructorDecl>(method) || !method->isVirtual())
            continue;

          if (auto dtor = dyn_cast<CXXDestructorDecl>(method)) {
            // Skip default destructor.
            if (dtor->isDefaulted())
              continue;
            addThunk({dtor, Dtor_Deleting});
            addThunk({dtor, Dtor_Complete});
          } else
            addThunk(method);
        }
      }
    }
  }

  if (!emittedVTable)
    return;

  if (hasRTTI(context, decl)) {
    auto name = getMangledCXXRTTI(decl);
    frontend.api.addGlobalVariable(name, StringRef{}, usr, loc, avail, access,
                                   docComment, {}, {}, nullptr,
                                   APILinkage::Exported);

    name = getMangledCXXRTTIName(decl);
    frontend.api.addGlobalVariable(name, StringRef{}, usr, loc, avail, access,
                                   docComment, {}, {}, nullptr,
                                   APILinkage::Exported);
  }

  for (const auto &it : decl->bases()) {
    const CXXRecordDecl *base =
        cast<CXXRecordDecl>(it.getType()->castAs<RecordType>()->getDecl());
    auto attributes = getFileAttributesForDecl(base);
    if (!attributes)
      continue;
    APIAccess baseAccess;
    PresumedLoc baseLoc;
    std::tie(baseAccess, baseLoc) = attributes.getValue();
    auto baseAvail = getAvailabilityInfo(base);
    emitVTableSymbols(base, baseLoc, baseAvail, baseAccess, true);
  }
}

bool APIVisitor::VisitRecordDecl(const RecordDecl *decl) {
  if (!decl->isCompleteDefinition())
    return true;

  // Skip C++ structs/classes/unions
  // FIXME: support C++ records later. Maybe merge with VisitCXXRecordDecl
  if (isa<CXXRecordDecl>(decl))
    return true;

  auto attributes = getFileAttributesForDecl(decl);
  if (!attributes)
    return true;
  auto name = decl->getName();
  if (name.empty())
    name = getTypedefName(decl);
  APIAccess access;
  PresumedLoc loc;
  std::tie(access, loc) = attributes.getValue();
  auto avail = getAvailabilityInfo(decl);
  SmallString<128> usr;
  clang::index::generateUSRForDecl(decl, usr);
  DocComment docComment(context.getRawCommentForDeclNoCache(decl),
                        context.getSourceManager(), context.getDiagnostics());
  auto *structRecord = frontend.api.addStruct(
      name, usr, loc, avail, access, docComment,
      DeclarationFragmentsBuilder::getFragmentsForStruct(decl),
      DeclarationFragmentsBuilder::getSubHeading(decl), decl);
  recordStructFields(structRecord, decl->fields());

  return true;
}

void APIVisitor::recordStructFields(StructRecord *record,
                                    const RecordDecl::field_range fields) {
  for (const auto *field : fields) {
    auto attributes = getFileAttributesForDecl(field);
    if (!attributes)
      continue;
    APIAccess access;
    PresumedLoc loc;
    std::tie(access, loc) = attributes.getValue();
    auto avail = getAvailabilityInfo(field);
    auto name = field->getName();
    SmallString<128> usr;
    clang::index::generateUSRForDecl(field, usr);
    DocComment docComment(context.getRawCommentForDeclNoCache(field),
                          context.getSourceManager(), context.getDiagnostics());
    frontend.api.addStructField(
        record, name, StringRef{}, usr, loc, avail, access, docComment,
        DeclarationFragmentsBuilder::getFragmentsForField(field),
        DeclarationFragmentsBuilder::getSubHeading(field), field);
  }
}

bool APIVisitor::VisitCXXRecordDecl(const CXXRecordDecl *decl) {
  // FIXME: generate declaration fragments as necessary when we support C++
  if (!decl->isCompleteDefinition())
    return true;

  // Skip templated classes.
  if (decl->getDescribedClassTemplate() != nullptr)
    return true;

  // Skip partial templated classes too.
  if (isa<ClassTemplatePartialSpecializationDecl>(decl))
    return true;

  auto attributes = getFileAttributesForDecl(decl);
  if (!attributes)
    return true;
  APIAccess access;
  PresumedLoc loc;
  std::tie(access, loc) = attributes.getValue();
  auto avail = getAvailabilityInfo(decl);

  // Check if we need to emit the vtable/rtti symbols.
  if (isExported(decl))
    emitVTableSymbols(decl, loc, avail, access);

  auto classSpecializationKind = TSK_Undeclared;
  bool keepInlineAsWeak = false;
  if (auto *templ = dyn_cast<ClassTemplateSpecializationDecl>(decl)) {
    classSpecializationKind = templ->getTemplateSpecializationKind();
    if (classSpecializationKind == TSK_ExplicitInstantiationDeclaration)
      keepInlineAsWeak = true;
  }

  // Record the class methods.
  for (const auto *method : decl->methods()) {
    // Inlined methods are usually not emitted - except it comes from a
    // specialized template.
    bool isWeakDef = false;
    if (isInlined(context, method)) {
      if (!keepInlineAsWeak)
        continue;

      isWeakDef = true;
    }

    // Skip the methods that are not exported.
    if (!isExported(method))
      continue;

    switch (method->getTemplateSpecializationKind()) {
    case TSK_Undeclared:
    case TSK_ExplicitSpecialization:
      break;
    case TSK_ImplicitInstantiation:
      continue;
    case TSK_ExplicitInstantiationDeclaration:
      if (classSpecializationKind == TSK_ExplicitInstantiationDeclaration)
        isWeakDef = true;
      break;
    case TSK_ExplicitInstantiationDefinition:
      isWeakDef = true;
      break;
    }

    if (!method->isUserProvided())
      continue;

    // Methods that are deleted are not exported.
    if (method->isDeleted())
      continue;

    // Abstract methods aren't exported either.
    if (method->isPure())
      continue;

    auto attributes = getFileAttributesForDecl(method);
    if (!attributes)
      return true;
    APIAccess access;
    PresumedLoc loc;
    // FIXME: for C++ ExtractAPI support, figure out the best way to handle declName
    // method->getName will fail, since these methods aren't simple identifiers
    std::tie(access, loc) = attributes.getValue();
    auto avail = getAvailabilityInfo(method);
    SmallString<128> usr;
    clang::index::generateUSRForDecl(method, usr);
    DocComment docComment(context.getRawCommentForDeclNoCache(method),
                          context.getSourceManager(), context.getDiagnostics());

    if (const auto *ctor = dyn_cast<CXXConstructorDecl>(method)) {
      // Defaulted constructors are not exported.
      if (ctor->isDefaulted())
        continue;

      auto name = getMangledCtorDtor(method, Ctor_Base);
      frontend.api.addFunction(name, StringRef{}, usr, loc, avail, access,
                               docComment, {}, {}, {}, nullptr,
                               APILinkage::Exported, isWeakDef);

      if (!decl->isAbstract()) {
        auto name = getMangledCtorDtor(method, Ctor_Complete);
        frontend.api.addFunction(name, StringRef{}, usr, loc, avail, access,
                                 docComment, {}, {}, {}, nullptr,
                                 APILinkage::Exported, isWeakDef);
      }

      continue;
    }

    if (const auto *dtor = dyn_cast<CXXDestructorDecl>(method)) {
      // Defaulted destructors are not exported.
      if (dtor->isDefaulted())
        continue;

      auto name = getMangledCtorDtor(method, Dtor_Base);
      frontend.api.addFunction(name, StringRef{}, usr, loc, avail, access,
                               docComment, {}, {}, {}, nullptr,
                               APILinkage::Exported, isWeakDef);

      name = getMangledCtorDtor(method, Dtor_Complete);
      frontend.api.addFunction(name, StringRef{}, usr, loc, avail, access,
                               docComment, {}, {}, {}, nullptr,
                               APILinkage::Exported, isWeakDef);

      if (dtor->isVirtual()) {
        auto name = getMangledCtorDtor(method, Dtor_Deleting);
        frontend.api.addFunction(name, StringRef{}, usr, loc, avail, access,
                                 docComment, {}, {}, {}, nullptr,
                                 APILinkage::Exported, isWeakDef);
      }

      continue;
    }

    auto name = getMangledName(method);
    frontend.api.addFunction(name, StringRef{}, usr, loc, avail, access,
                             docComment, {}, {}, {}, nullptr,
                             APILinkage::Exported, isWeakDef);
  }

  if (auto *templ = dyn_cast<ClassTemplateSpecializationDecl>(decl)) {
    if (!templ->isExplicitInstantiationOrSpecialization())
      return true;
  }

  using var_iter = CXXRecordDecl::specific_decl_iterator<VarDecl>;
  using var_range = iterator_range<var_iter>;
  for (auto *var : var_range(decl->decls())) {
    // Skip const static member variables.
    // \code
    // struct S {
    //   static const int x = 0;
    // };
    // \endcode
    if (var->isStaticDataMember() && var->hasInit())
      continue;

    // Skip unexported var decls.
    if (!isExported(var))
      continue;

    auto name = getMangledName(var);
    auto declName = var->getName();
    auto attributes = getFileAttributesForDecl(var);
    if (!attributes)
      return true;
    APIAccess access;
    PresumedLoc loc;
    std::tie(access, loc) = attributes.getValue();
    auto avail = getAvailabilityInfo(var);
    bool isWeakDef = var->hasAttr<WeakAttr>() || keepInlineAsWeak;
    SmallString<128> usr;
    clang::index::generateUSRForDecl(var, usr);
    DocComment docComment(context.getRawCommentForDeclNoCache(var),
                          context.getSourceManager(), context.getDiagnostics());

    frontend.api.addGlobalVariable(name, declName, usr, loc, avail, access,
                                   docComment, {}, {}, var,
                                   APILinkage::Exported, isWeakDef);
  }

  return true;
}

bool APIVisitor::VisitTypedefNameDecl(const TypedefNameDecl *decl) {
  // Skip ObjC Type Parameter for now.
  if (isa<ObjCTypeParamDecl>(decl))
   return true;

  if (!decl->isDefinedOutsideFunctionOrMethod())
    return true;

  auto attributes = getFileAttributesForDecl(decl);
  if (!attributes)
    return true;
  APIAccess access;
  PresumedLoc loc;
  std::tie(access, loc) = attributes.getValue();
  auto name = decl->getName();
  auto avail = getAvailabilityInfo(decl);
  SmallString<128> usr;
  clang::index::generateUSRForDecl(decl, usr);

  QualType type = decl->getUnderlyingType();
  SymbolInfo info = getUnderlyingTypeInfo(type, context, &frontend.api);

  DocComment docComment(context.getRawCommentForDeclNoCache(decl),
                        context.getSourceManager(), context.getDiagnostics());

  frontend.api.addTypeDef(
      name, usr, loc, avail, access, info, docComment,
      DeclarationFragmentsBuilder::getFragmentsForTypedef(decl),
      DeclarationFragmentsBuilder::getSubHeading(decl), decl);

  return true;
}

StringRef APIVisitor::getSourceModule(const Decl *decl,
                                      clang::SourceManager &sourceManager) {
  if (const clang::Module *mod = decl->getOwningModule())
    return mod->Name;

  clang::SourceLocation loc = decl->getLocation();
  if (loc.isInvalid())
    return {};

  // If the loc refers to a macro expansion we need to first get the file
  // location of the expansion.
  clang::SourceLocation fileLoc = sourceManager.getFileLoc(loc);
  clang::FileID id = sourceManager.getFileID(fileLoc);
  if (id.isInvalid())
    return {};

  auto filename = sourceManager.getNonBuiltinFilenameForID(id);
  if (!filename)
    return {};

  const llvm::Regex rule("/([^/]+)\\.framework/");
  SmallVector<StringRef, 2> matches;
  rule.match(*filename, &matches);
  if (matches.size() != 2)
    return {};
  return matches[1];
}

SymbolInfo APIVisitor::getUnderlyingTypeInfo(const QualType type,
                                             ASTContext &context, API *api) {
  std::string typeName = type.getAsString();
  SmallString<128> typeUSR;
  std::string sourceModule;
  const NamedDecl *typeDecl = nullptr;

  const TypedefType *typedefType = type->getAs<TypedefType>();

  if (const TagType *tagType = type->getAs<TagType>()) {
    typeDecl = tagType->getDecl();
  } else if (const ObjCInterfaceType *objCIType =
                 type->getAs<ObjCInterfaceType>()) {
    typeDecl = objCIType->getDecl();
  }

  if (typeDecl) {
    // If our underlying type is pointing at another typedef, don't look for the
    // (potentially anonymous) underlying type - that could cause us to
    // erroneously drop this decl.
    if (!typedefType) {
      typeName = typeDecl->getName().str();
    } else {
      // if this is a typedef to another typedef, use the typedef's decl for the
      // USR - this will actually be in the output, unlike a typedef to an
      // anonymous decl
      const TypedefNameDecl *typedefDecl = typedefType->getDecl();
      if (typedefDecl->getUnderlyingType()->isTypedefNameType())
        typeDecl = typedefDecl;
    }

    clang::index::generateUSRForDecl(typeDecl, typeUSR);
    sourceModule = getSourceModule(typeDecl, context.getSourceManager()).str();
  } else {
    clang::index::generateUSRForType(type, context, typeUSR);
  }

  SymbolInfo info(typeName, typeUSR, sourceModule);

  // Copy the string memory into a stable location so that they can be used
  // outside this function
  if (api)
    return info.copied(*api);
  else
    return info.copiedInto(context.getAllocator());
}

void MacroCallback::MacroDefined(const Token &macroNameToken,
                                 const MacroDirective *md) {
  if (md->getMacroInfo()->isBuiltinMacro())
    return;

  if (md->getMacroInfo()->isUsedForHeaderGuard())
    return;

  auto attributes =
      getFileAttributesForLoc(context, macroNameToken.getLocation());
  if (!attributes)
    return;

  APIAccess access;
  PresumedLoc loc;
  std::tie(access, loc) = attributes.getValue();

  SmallString<128> macroUSR;
  clang::index::generateUSRForMacro(
      macroNameToken.getIdentifierInfo()->getName(),
      macroNameToken.getLocation(), *context.sourceMgr, macroUSR);

  context.api.addMacroDefinition(
      macroNameToken.getIdentifierInfo()->getName(), macroUSR, loc, access,
      DeclarationFragmentsBuilder::getFragmentsForMacro(macroNameToken, md));
}

} // end namespace clang.
