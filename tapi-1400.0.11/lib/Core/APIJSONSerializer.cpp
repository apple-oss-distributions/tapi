//===- lib/Core/APIJSONSerializer.cpp - TAPI API JSONSerializer -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "tapi/Core/APIJSONSerializer.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::json;
using namespace clang;

TAPI_NAMESPACE_INTERNAL_BEGIN

class APIJSONVisitor : public APIVisitor {
public:
  APIJSONVisitor(const APIJSONOption &options) : options(options) {}
  ~APIJSONVisitor() override {}

  friend APIJSONSerializer;

  void visitMacroDefinition(const MacroDefinitionRecord &) override;
  void visitGlobal(const GlobalRecord &) override;
  void visitEnum(const EnumRecord &) override;
  void visitObjCInterface(const ObjCInterfaceRecord &) override;
  void visitObjCCategory(const ObjCCategoryRecord &) override;
  void visitObjCProtocol(const ObjCProtocolRecord &) override;
  void visitTypeDef(const TypedefRecord &) override;
  void visitStruct(const StructRecord &) override;

private:
  const APIJSONOption &options;
  Array macros;
  Array globals;
  Array interfaces;
  Array categories;
  Array protocols;
  Array enums;
  Array typedefs;
  Array structs;
};

class APIJSONParser {
public:
  APIJSONParser(API &api, bool publicOnly = false)
      : result(api), publicOnly(publicOnly) {}
  Error parse(Object *root);

private:
  // top level parsers.
  Error parseMacros(Array &macros);
  Error parseGlobals(Array &globals);
  Error parseInterfaces(Array &interfaces);
  Error parseCategories(Array &categories);
  Error parseProtocols(Array &protocols);
  Error parseEnums(Array &enums);
  Error parseTypedefs(Array &types);
  Error parseStructs(Array &structs);
  Error parseBinaryInfo(Object &binaryInfo);
  Error parsePotentiallyDefinedSelectors(Array &selectors);

  // helper functions.
  Expected<StringRef> parseString(StringRef key, const Object *obj,
                                  llvm::Twine error);
  bool parseBinaryField(StringRef key, const Object *obj);

  // parse structs.
  Expected<StringRef> parseName(const Object *obj);
  Expected<APILoc> parseLocation(const Object *obj);
  Expected<AvailabilityInfo> parseAvailability(const Object *obj);
  Expected<APIAccess> parseAccess(const Object *obj);
  Expected<GVKind> parseGlobalKind(const Object *obj);
  Expected<APILinkage> parseLinkage(const Object *obj);
  APIFlags parseFlags(const Object *obj);
  StringRef parseUSR(const Object *obj);
  StringRef parseDeclName(const Object *obj);
  Expected<APIRange> parseRange(const Object *obj, const StringRef file);
  Expected<DocComment> parseDocComment(const Object *obj);
  Expected<DeclarationFragments> parseDeclarationFragmentsFrom(const Array *array);
  Expected<DeclarationFragments> parseDeclarationFragments(const Object *obj);
  Expected<FunctionSignature> parseFunctionSignature(const Object *obj);
  Expected<DeclarationFragments> parseSubHeading(const Object *obj);
  SymbolInfo parseSymbolInfo(const Value *value);
  SymbolInfo parseSuperClass(const Object *obj);
  SymbolInfo parseCategoryInterface(const Object *obj);
  SymbolInfo parseUnderlyingType(const Object *obj);

  using PAKind = ObjCPropertyRecord::AttributeKind;
  friend inline APIJSONParser::PAKind operator|=(APIJSONParser::PAKind &a,
                                                 const APIJSONParser::PAKind b);
  Expected<PAKind> parsePropertyAttribute(const Object *obj);
  Expected<ObjCInstanceVariableRecord::AccessControl>
  parseAccessControl(const Object *obj);

  // parse sub objects.
  Error parseConformedProtocols(ObjCContainerRecord *container,
                                const Object *obj);
  Error parseMethods(ObjCContainerRecord *container, const Object *object,
                     bool isInstanceMethod);
  Error parseProperties(ObjCContainerRecord *container, const Object *object);
  Error parseIvars(ObjCContainerRecord *container, const Object *object);
  Error parseEnumConstants(EnumRecord *record, const Object *object);
  Error parseStructFields(StructRecord *record, const Object *object);

  // references to output.
  API &result;

  bool publicOnly;
};

// Helper to write boolean value. Default is to skip false boolean value.
static void serializeBoolean(Object &obj, StringRef key, bool value) {
  if (!value)
    return;
  obj[key] = true;
}

// Helper to write array. Default is to skip empty array.
static void serializeArray(Object &obj, StringRef key,
                           const std::vector<StringRef> array) {
  if (array.empty())
    return;
  Array values;
  for (const auto &e : array)
    values.emplace_back(e);
  obj[key] = std::move(values);
}

static void serializeLocation(Object &obj, const APILoc &loc,
                              const APIJSONOption &options) {
  // skip invalid location.
  if (loc.isInvalid())
    return;

  obj["file"] = loc.getFilename().str();
  if (options.ignoreLineCol)
    return;

  obj["line"] = loc.getLine();
  obj["col"] = loc.getColumn();
}

static std::string getStringForPackedVersion(const PackedVersion &version) {
  std::string str;
  raw_string_ostream rss(str);
  rss << version;
  return rss.str();
}

static void serializeAvailability(Object &obj, const AvailabilityInfo &avail,
                                  const APIJSONOption &option) {
  if (avail.isDefault())
    return;

  if (avail._introduced != PackedVersion())
    obj["introduced"] = getStringForPackedVersion(avail._introduced);
  if (!option.noDeprecationInfo && avail._deprecated != PackedVersion())
    obj["deprecated"] = getStringForPackedVersion(avail._deprecated);
  if (avail._obsoleted != PackedVersion())
    obj["obsoleted"] = getStringForPackedVersion(avail._obsoleted);
  if (avail.isUnavailable())
    obj["unavailable"] = true;
  if (!option.noDeprecationInfo && avail.isUnconditionallyDeprecated())
    obj["unconditionallyDeprecated"] = true;
  if (avail.isSPIAvailable())
    obj["SPIAvailable"] = true;
}

static void serializeLinkage(Object &obj, APILinkage linkage) {
  switch (linkage) {
  case APILinkage::Exported:
    obj["linkage"] = "exported";
    break;
  case APILinkage::Reexported:
    obj["linkage"] = "re-exported";
    break;
  case APILinkage::Internal:
    obj["linkage"] = "internal";
    break;
  case APILinkage::External:
    obj["linkage"] = "external";
    break;
  case APILinkage::Unknown:
    // do nothing;
    break;
  }
}

static void serializeUSR(Object &obj, StringRef usr,
                         const APIJSONOption &option) {
  if (usr.empty() || option.noUSR)
    return;

  obj["USR"] = usr.str();
}

static void serializeSymbolInfo(Object &obj, SymbolInfo symbol,
                                const APIJSONOption &option) {
  if (symbol.empty())
    return;

  if (!symbol.name.empty())
    obj["name"] = symbol.name.str();
  serializeUSR(obj, symbol.usr, option);
  if (!symbol.sourceModule.empty())
    obj["sourceModule"] = symbol.sourceModule.str();
}

static void serializeFlags(Object &obj, APIFlags flags) {
  serializeBoolean(obj, "weakDefined",
                   (flags & APIFlags::WeakDefined) == APIFlags::WeakDefined);
  serializeBoolean(obj, "weakReferenced",
                   (flags & APIFlags::WeakReferenced) ==
                       APIFlags::WeakReferenced);
  serializeBoolean(obj, "threadLocalValue",
                   (flags & APIFlags::ThreadLocalValue) ==
                       APIFlags::ThreadLocalValue);
}

static void serializeRange(Object &obj, const APIRange &range, bool noFile) {
  Object start, end;
  APILoc startLoc = range.getBegin(), endLoc = range.getEnd();

  // skip invalid ranges
  if (startLoc.isInvalid() || endLoc.isInvalid())
    return;

  if (!noFile) {
    start["file"] = startLoc.getFilename().str();
    end["file"] = endLoc.getFilename().str();
  }

  start["line"] = startLoc.getLine();
  start["col"] = startLoc.getColumn();
  end["line"] = endLoc.getLine();
  end["col"] = endLoc.getColumn();

  obj["start"] = std::move(start);
  obj["end"] = std::move(end);
}

static void serializeDocComment(Object &obj, const DocComment &docComment,
                                const APIJSONOption &option) {
  if (option.noDocComment || docComment.getCommentLines().empty())
    return;

  Object lines;
  Array commentLines;
  for (const auto &commentLine : docComment.getCommentLines()) {
    Object range, line;
    serializeRange(range, commentLine.range, /*noFile=*/true);
    line["text"] = commentLine.text;
    line["range"] = std::move(range);
    commentLines.emplace_back(std::move(line));
  }
  lines["lines"] = std::move(commentLines);
  obj["docComment"] = std::move(lines);
}

static void serializeDeclName(Object &obj, const APIRecord &var,
                              const APIJSONOption &option) {
  if (var.declName.empty() || option.noDeclName)
    return;

  if (var.declName == var.name)
    return;

  obj["declName"] = var.declName;
}

static void
serializeDeclarationFragmentsInto(Array &fragments,
                                  const DeclarationFragments &declarationFragments) {
  for (const auto &declarationFragment : declarationFragments.getFragments()) {
    Object fragment;
    fragment["spelling"] = declarationFragment.spelling;
    fragment["kind"] =
        DeclarationFragments::getFragmentKindString(declarationFragment.kind);
    if (!declarationFragment.preciseIdentifier.empty())
      fragment["preciseIdentifier"] = declarationFragment.preciseIdentifier;
    fragments.emplace_back(std::move(fragment));
  }
}

static void
serializeDeclarationFragments(Object &obj,
                              const DeclarationFragments &declarationFragments,
                              const APIJSONOption &option) {
  if (option.noDeclFragments || declarationFragments.getFragments().empty())
    return;

  Array fragments;
  serializeDeclarationFragmentsInto(fragments, declarationFragments);
  obj["declarationFragments"] = std::move(fragments);
}

static void
serializeFunctionSignature(Object &obj,
                           const FunctionSignature &functionSignature,
                           const APIJSONOption &option) {
  if (option.noDeclFragments || functionSignature.empty())
    return;

  Object signatureObject;

  Array returnType;
  serializeDeclarationFragmentsInto(returnType, functionSignature.getReturnType());
  if (!returnType.empty())
    signatureObject["returns"] = std::move(returnType);

  Array parameters;
  for (const auto &parameter : functionSignature.getParameters()) {
    Object param;
    param["name"] = parameter.name;

    Array parameterFragments;
    serializeDeclarationFragmentsInto(parameterFragments, parameter.declarationFragments);
    param["declarationFragments"] = std::move(parameterFragments);

    parameters.emplace_back(std::move(param));
  }

  if (!parameters.empty())
    signatureObject["parameters"] = std::move(parameters);

  obj["functionSignature"] = std::move(signatureObject);
}

static void serializeSubHeading(Object &obj,
                                const DeclarationFragments &subHeading,
                                const APIJSONOption &option) {
  if (option.noDeclFragments || subHeading.getFragments().empty())
    return;

  Array fragments;
  serializeDeclarationFragmentsInto(fragments, subHeading);
  obj["subHeading"] = std::move(fragments);
}

static Optional<Object> serializeAPIRecord(const APIRecord &var,
                                           const APIJSONOption &options) {
  if (options.publicOnly && var.access != APIAccess::Public)
    return llvm::None;

  Object obj;
  obj["name"] = var.name;

  serializeDeclName(obj, var, options);
  serializeUSR(obj, var.usr, options);
  serializeLocation(obj, var.loc, options);
  serializeAvailability(obj, var.availability, options);
  serializeLinkage(obj, var.linkage);
  serializeFlags(obj, var.flags);
  serializeDocComment(obj, var.docComment, options);
  serializeDeclarationFragments(obj, var.declarationFragments, options);
  serializeSubHeading(obj, var.subHeading, options);

  // When only public, all APIs in the output are public so there is no need to
  // encode access.
  if (options.publicOnly)
    return obj;

  switch (var.access) {
  case APIAccess::Public:
    obj["access"] = "public";
    break;
  case APIAccess::Private:
    obj["access"] = "private";
    break;
  case APIAccess::Project:
    obj["access"] = "project";
    break;
  case APIAccess::Unknown:
    // do nothing;
    break;
  }

  return obj;
}

static Optional<Object> serializeGlobalRecord(const GlobalRecord &var,
                                              const APIJSONOption &options) {
  auto obj = serializeAPIRecord(var, options);
  if (!obj)
    return llvm::None;

  switch (var.kind) {
  case GVKind::Function:
    obj.getValue()["kind"] = "function";
    break;
  case GVKind::Variable:
    obj.getValue()["kind"] = "variable";
    break;
  case GVKind::Unknown:
    // do nothing;
    break;
  }

  serializeFunctionSignature(*obj, var.functionSignature, options);

  return std::move(*obj);
}

static void serializeMethod(Array &container, const ObjCMethodRecord *method,
                            const APIJSONOption &options) {

  auto obj = serializeAPIRecord(*method, options);
  if (!obj)
    return;

  serializeFunctionSignature(*obj, method->signature, options);

  serializeBoolean(*obj, "optional", method->isOptional);
  serializeBoolean(*obj, "dynamic", method->isDynamic);
  container.emplace_back(std::move(*obj));
}

static void serializeProperty(Array &container,
                              const ObjCPropertyRecord *property,
                              const APIJSONOption &options) {
  auto obj = serializeAPIRecord(*property, options);
  if (!obj)
    return;

  Array attributes;
  if (property->isReadOnly())
    attributes.emplace_back("readonly");
  if (property->isDynamic())
    attributes.emplace_back("dynamic");
  if (property->isClassProperty())
    attributes.emplace_back("class");
  if (!attributes.empty())
    obj.getValue()["attr"] = std::move(attributes);

  serializeBoolean(*obj, "optional", property->isOptional);
  obj.getValue()["getter"] = property->getterName;

  if (!property->isReadOnly())
    obj.getValue()["setter"] = property->setterName;

  container.emplace_back(std::move(*obj));
}

static void serializeInstanceVariable(Array &container,
                                      const ObjCInstanceVariableRecord *ivar,
                                      const APIJSONOption &options) {
  auto obj = serializeAPIRecord(*ivar, options);
  if (!obj)
    return;

  switch (ivar->accessControl) {
  case ObjCInstanceVariableRecord::AccessControl::Private:
    obj.getValue()["accessControl"] = "private";
    break;
  case ObjCInstanceVariableRecord::AccessControl::Protected:
    obj.getValue()["accessControl"] = "protected";
    break;
  case ObjCInstanceVariableRecord::AccessControl::Public:
    obj.getValue()["accessControl"] = "public";
    break;
  case ObjCInstanceVariableRecord::AccessControl::Package:
    obj.getValue()["accessControl"] = "package";
    break;
  case ObjCInstanceVariableRecord::AccessControl::None:
    break; // ignore;
  }

  container.emplace_back(std::move(*obj));
}

void APIJSONVisitor::visitMacroDefinition(const MacroDefinitionRecord &record) {
  if (options.noMacroDefinitions)
    return;

  auto root = serializeAPIRecord(record, options);
  if (!root)
    return;

  macros.emplace_back(std::move(*root));
}

void APIJSONVisitor::visitGlobal(const GlobalRecord &record) {
  if (options.externalOnly && !record.isExported())
    return;

  auto root = serializeGlobalRecord(record, options);
  if (!root)
    return;

  globals.emplace_back(std::move(*root));
}

static Optional<Object>
serializeObjCContainer(const ObjCContainerRecord &record,
                       const APIJSONOption &options) {
  auto root = serializeAPIRecord(record, options);
  if (!root)
    return llvm::None;

  if (!record.protocols.empty()) {
    Array protocols;
    for (const auto &protocol : record.protocols) {
      if (options.noElaboratedSymbolInfo) {
        if (!protocol.name.empty())
          protocols.emplace_back(protocol.name.str());
      } else {
        Object obj;
        serializeSymbolInfo(obj, protocol, options);
        protocols.emplace_back(std::move(obj));
      }
    }
    root.getValue()["protocols"] = std::move(protocols);
  }

  if (!record.methods.empty()) {
    Array instanceMethodRoot, classMethodRoot;
    for (const auto *method : record.methods) {
      if (method->isInstanceMethod)
        serializeMethod(instanceMethodRoot, method, options);
      else
        serializeMethod(classMethodRoot, method, options);
    }
    if (!instanceMethodRoot.empty())
      root.getValue()["instanceMethods"] = std::move(instanceMethodRoot);
    if (!classMethodRoot.empty())
      root.getValue()["classMethods"] = std::move(classMethodRoot);
  }

  if (!record.properties.empty()) {
    Array propertyRoot;
    for (const auto *property : record.properties)
      serializeProperty(propertyRoot, property, options);
    root.getValue()["properties"] = std::move(propertyRoot);
  }

  if (!record.ivars.empty()) {
    Array ivarRoot;
    for (const auto *ivar : record.ivars)
      serializeInstanceVariable(ivarRoot, ivar, options);
    root.getValue()["ivars"] = std::move(ivarRoot);
  }

  return root;
}

void APIJSONVisitor::visitObjCInterface(const ObjCInterfaceRecord &interface) {
  if (options.externalOnly && !interface.isExported())
    return;

  auto root = serializeObjCContainer(interface, options);
  if (!root)
    return;

  root.getValue()["super"] = interface.superClass.name.str();
  if (!options.noElaboratedSymbolInfo) {
    if (!interface.superClass.usr.empty())
      root.getValue()["superUSR"] = interface.superClass.usr.str();
    if (!interface.superClass.sourceModule.empty())
      root.getValue()["superSourceModule"] =
          interface.superClass.sourceModule.str();
  }

  serializeLinkage(*root, interface.linkage);
  serializeBoolean(*root, "hasException", interface.hasExceptionAttribute);

  if (!interface.categories.empty()) {
    Array categories;
    for (const auto *category : interface.categories)
      if (!category->name.empty())
        categories.emplace_back(category->name);
    root.getValue()["categories"] = std::move(categories);
  }

  interfaces.emplace_back(std::move(*root));
}

void APIJSONVisitor::visitObjCCategory(const ObjCCategoryRecord &category) {
  auto root = serializeObjCContainer(category, options);
  if (!root)
    return;

  root.getValue()["interface"] = category.interface.name.str();
  if (!options.noElaboratedSymbolInfo) {
    if (!category.interface.usr.empty())
      root.getValue()["interfaceUSR"] = category.interface.usr.str();
    if (!category.interface.sourceModule.empty())
      root.getValue()["interfaceSourceModule"] =
          category.interface.sourceModule.str();
  }

  categories.emplace_back(std::move(*root));
}

void APIJSONVisitor::visitObjCProtocol(const ObjCProtocolRecord &protocol) {
  auto root = serializeObjCContainer(protocol, options);
  if (root)
    protocols.emplace_back(std::move(*root));
}

static Optional<Object> serializeEnumRecord(const EnumRecord &record,
                                            const APIJSONOption &options) {
  auto root = serializeAPIRecord(record, options);
  if (!root)
    return llvm::None;

  if (!record.constants.empty()) {
    Array constants;
    for (const auto *constant : record.constants) {
      auto constantObj = serializeAPIRecord(*constant, options);
      if (!constantObj)
        continue;
      constants.emplace_back(std::move(*constantObj));
    }
    root.getValue()["constants"] = std::move(constants);
  }

  return std::move(*root);
}

void APIJSONVisitor::visitEnum(const EnumRecord &record) {
  auto root = serializeEnumRecord(record, options);
  if (!root)
    return;

  enums.emplace_back(std::move(*root));
}

static Optional<Object> serializeTypedefRecord(const TypedefRecord &record,
                                               const APIJSONOption &options) {
  // Typedefs of anonymous types have their entries unified with the underlying
  // type.
  if (!options.noUnifiedTypedefEntries && record.underlyingType.name.empty())
    return llvm::None;

  auto root = serializeAPIRecord(record, options);
  if (!root)
    return llvm::None;

  if (!options.noElaboratedSymbolInfo && !record.underlyingType.empty()) {
    if (!record.underlyingType.name.empty())
      root.getValue()["type"] = record.underlyingType.name.str();
    if (!record.underlyingType.usr.empty())
      root.getValue()["typeUSR"] = record.underlyingType.usr.str();
    if (!record.underlyingType.sourceModule.empty())
      root.getValue()["typeSourceModule"] =
          record.underlyingType.sourceModule.str();
  }

  return std::move(*root);
}

void APIJSONVisitor::visitTypeDef(const TypedefRecord &record) {
  auto root = serializeTypedefRecord(record, options);
  if (root)
    typedefs.emplace_back(std::move(*root));
}

static std::string getPackedVersionString(PackedVersion version) {
  std::string str;
  raw_string_ostream vers(str);
  vers << version;
  return vers.str();
}

static Optional<Object> serializeStructRecord(const StructRecord &record,
                                              const APIJSONOption &options) {
  auto root = serializeAPIRecord(record, options);
  if (!root)
    return llvm::None;

  if (!record.fields.empty()) {
    Array fields;
    for (const auto *field : record.fields) {
      auto fieldObj = serializeAPIRecord(*field, options);
      if (!fieldObj)
        continue;
      fields.emplace_back(std::move(*fieldObj));
    }
    root.getValue()["fields"] = std::move(fields);
  }

  return std::move(*root);
}

void APIJSONVisitor::visitStruct(const StructRecord &record) {
  if (options.noStruct ||
      (options.publicOnly && record.access != APIAccess::Public))
    return;

  auto root = serializeStructRecord(record, options);
  if (!root)
    return;

  structs.emplace_back(std::move(*root));
}

static void serializeBinaryInfo(Object &root, const BinaryInfo &binaryInfo,
                                bool noUUID) {
  Object info;
  switch (binaryInfo.fileType) {
  case FileType::MachO_DynamicLibrary:
    info["type"] = "dylib";
    break;
  case FileType::MachO_DynamicLibrary_Stub:
    info["type"] = "stub";
    break;
  case FileType::MachO_Bundle:
    info["type"] = "bundle";
    break;
  default:
    // All other file types are invalid.
    info["type"] = "invalid";
    break;
  }
  info["currentVersion"] = getPackedVersionString(binaryInfo.currentVersion);
  info["compatibilityVersion"] =
      getPackedVersionString(binaryInfo.compatibilityVersion);
  info["installName"] = binaryInfo.installName;
  if (!noUUID)
    info["uuid"] = binaryInfo.uuid;
  // Optional fields below.
  if (!binaryInfo.parentUmbrella.empty())
    info["parentUmbrella"] = binaryInfo.parentUmbrella;
  if (binaryInfo.swiftABIVersion)
    info["swiftABI"] = binaryInfo.swiftABIVersion;
  serializeBoolean(info, "twoLevelNamespace", binaryInfo.isTwoLevelNamespace);
  serializeBoolean(info, "appExtensionSafe", binaryInfo.isAppExtensionSafe);
  serializeArray(info, "allowableClients", binaryInfo.allowableClients);
  serializeArray(info, "reexportedLibraries", binaryInfo.reexportedLibraries);
  root["binaryInfo"] = std::move(info);
}

static void serializePotentiallyDefinedSelectors(
    Object &root, const StringSet<> &potentiallyDefinedSelectors) {
  if (potentiallyDefinedSelectors.empty())
    return;

  std::vector<StringRef> selectors;
  for (const auto &s : potentiallyDefinedSelectors)
    selectors.emplace_back(s.first());

  // sort the selectors for reproducibility.
  llvm::sort(selectors);

  Array output;
  for (auto s : selectors)
    output.emplace_back(s);

  root["potentiallyDefinedSelectors"] = std::move(output);
}

Object APIJSONSerializer::getJSONObject() const {
  json::Object root;
  if (!options.noTarget)
    root["target"] = std::move(api.getTarget().str());

  if (!api.getProjectName().empty())
    root["project"] = api.getProjectName().str();

  APIJSONVisitor visitor(options);
  api.visit(visitor);

  auto insertNonEmptyArray = [&](StringRef key, Array array) {
    if (array.empty())
      return;
    root[key] = std::move(array);
  };
  insertNonEmptyArray("macros", visitor.macros);
  insertNonEmptyArray("globals", visitor.globals);
  insertNonEmptyArray("interfaces", visitor.interfaces);
  insertNonEmptyArray("categories", visitor.categories);
  insertNonEmptyArray("protocols", visitor.protocols);
  insertNonEmptyArray("enums", visitor.enums);
  insertNonEmptyArray("typedefs", visitor.typedefs);
  if (!options.noStruct)
    insertNonEmptyArray("structs", visitor.structs);
  serializePotentiallyDefinedSelectors(root,
                                       api.getPotentiallyDefinedSelectors());
  if (api.hasBinaryInfo())
    serializeBinaryInfo(root, api.getBinaryInfo(), options.noUUID);
  return root;
}

void APIJSONSerializer::serialize(raw_ostream &os) const {
  auto root = getJSONObject();
  root["api_json_version"] = 1;
  if (options.compact)
    os << formatv("{0}", Value(std::move(root))) << "\n";
  else
    os << formatv("{0:2}", Value(std::move(root))) << "\n";
}

bool APIJSONParser::parseBinaryField(StringRef key, const Object *obj) {
  auto field = obj->getBoolean(key);
  if (!field)
    return false;
  return *field;
}

Expected<StringRef> APIJSONParser::parseString(StringRef key, const Object *obj,
                                               llvm::Twine error) {
  auto str = obj->getString(key);
  if (!str)
    return make_error<APIJSONError>(error);
  return *str;
}

Expected<StringRef> APIJSONParser::parseName(const Object *obj) {
  return parseString("name", obj, "missing name in json api object");
}

Expected<APILoc> APIJSONParser::parseLocation(const Object *obj) {
  auto file = obj->getString("file");
  if (!file)
    return APILoc();

  auto line = obj->getInteger("line");
  auto col = obj->getInteger("col");

  return APILoc(file->str(), line ? *line : 0, col ? *col : 0);
}

Expected<AvailabilityInfo> APIJSONParser::parseAvailability(const Object *obj) {
  auto intro = obj->getString("introduced");
  auto dep = obj->getString("deprecated");
  auto obs = obj->getString("obsoleted");
  auto unavail = obj->getBoolean("unavailable");
  auto isUnconditionallyDeprecated =
      obj->getBoolean("unconditionallyDeprecated");
  auto isSPIAvailable = obj->getBoolean("SPIAvailable");

  if (!intro && !dep && !obs && !unavail && !isSPIAvailable)
    return AvailabilityInfo();

  PackedVersion introduced, deprecated, obsoleted;
  if (intro && !introduced.parse32(*intro))
    return make_error<APIJSONError>("malformed introduced version");
  if (dep && !deprecated.parse32(*dep))
    return make_error<APIJSONError>("malformed deprecated version");
  if (obs && !obsoleted.parse32(*obs))
    return make_error<APIJSONError>("malformed obsoleted version");

  return AvailabilityInfo(
      introduced, deprecated, obsoleted, unavail ? *unavail : false,
      isUnconditionallyDeprecated ? *isUnconditionallyDeprecated : false,
      isSPIAvailable ? *isSPIAvailable : false);
}

Expected<APIAccess> APIJSONParser::parseAccess(const Object *obj) {
  auto access = obj->getString("access");
  if (!access)
    return publicOnly ? APIAccess::Public : APIAccess::Unknown;

  if (*access == "public")
    return APIAccess::Public;
  if (*access == "private")
    return APIAccess::Private;
  if (*access == "project")
    return APIAccess::Project;

  return make_error<APIJSONError>("Unknown access " + *access);
}

Expected<GVKind> APIJSONParser::parseGlobalKind(const Object *obj) {
  auto kind = obj->getString("kind");
  if (!kind)
    return GVKind::Unknown;

  if (*kind == "function")
    return GVKind::Function;
  if (*kind == "variable")
    return GVKind::Variable;

  return make_error<APIJSONError>("Unknown GVKind " + *kind);
}

Expected<APILinkage> APIJSONParser::parseLinkage(const Object *obj) {
  auto linkage = obj->getString("linkage");
  if (!linkage)
    return APILinkage::Unknown;

  if (*linkage == "exported")
    return APILinkage::Exported;
  if (*linkage == "re-exported")
    return APILinkage::Reexported;
  if (*linkage == "internal")
    return APILinkage::Internal;
  if (*linkage == "external")
    return APILinkage::External;

  return make_error<APIJSONError>("Unknown Linkage " + *linkage);
}

APIFlags APIJSONParser::parseFlags(const Object *obj) {
  auto isWeakDefined = parseBinaryField("weakDefined", obj);
  auto isWeakReferenced = parseBinaryField("weakReferenced", obj);
  auto isThreadLocal = parseBinaryField("threadLocalValue", obj);

  auto flags = APIFlags::None;
  if (isWeakDefined)
    flags |= APIFlags::WeakDefined;
  if (isWeakReferenced)
    flags |= APIFlags::WeakReferenced;
  if (isThreadLocal)
    flags |= APIFlags::ThreadLocalValue;

  return flags;
}

inline APIJSONParser::PAKind operator|=(APIJSONParser::PAKind &a,
                                        const APIJSONParser::PAKind b) {
  return a = (APIJSONParser::PAKind)(a | b);
}

Expected<APIJSONParser::PAKind>
APIJSONParser::parsePropertyAttribute(const Object *obj) {
  PAKind kind = PAKind::NoAttr;
  const auto *attrs = obj->getArray("attr");
  if (!attrs)
    return kind;

  for (const auto &attr : *attrs) {
    auto current = attr.getAsString();
    if (!current)
      return make_error<APIJSONError>("attribute should be string");

    if (*current == "readonly")
      kind |= PAKind::ReadOnly;
    else if (*current == "dynamic")
      kind |= PAKind::Dynamic;
    else if (*current == "class")
      kind |= PAKind::Class;
    else
      return make_error<APIJSONError>("Unknown property attr " + *current);
  }
  return kind;
}

Expected<ObjCInstanceVariableRecord::AccessControl>
APIJSONParser::parseAccessControl(const Object *obj) {
  auto access = obj->getString("access");
  if (!access)
    return ObjCInstanceVariableRecord::AccessControl::None;

  if (*access == "private")
    return ObjCInstanceVariableRecord::AccessControl::Private;
  if (*access == "protected")
    return ObjCInstanceVariableRecord::AccessControl::Protected;
  if (*access == "public")
    return ObjCInstanceVariableRecord::AccessControl::Protected;
  if (*access == "package")
    return ObjCInstanceVariableRecord::AccessControl::Protected;

  return make_error<APIJSONError>("Unknown access control " + *access);
}

StringRef APIJSONParser::parseUSR(const Object *obj) {
  return obj->getString("USR").getValueOr("");
}

StringRef APIJSONParser::parseDeclName(const Object *obj) {
  return obj->getString("declName").getValueOr("");
}

Expected<APIRange> APIJSONParser::parseRange(const Object *obj,
                                             const StringRef file) {
  const Object *start = obj->getObject("start"), *end = obj->getObject("end");

  if (!start || !end)
    return make_error<APIJSONError>("Missing 'start' or 'end' in range");

  auto startLine = start->getInteger("line");
  auto startCol = start->getInteger("col");
  APILoc startLoc(file, startLine ? *startLine : 0, startCol ? *startCol : 0);

  auto endLine = end->getInteger("line");
  auto endCol = end->getInteger("col");
  APILoc endLoc(file, endLine ? *endLine : 0, endCol ? *endCol : 0);

  return APIRange(startLoc, endLoc);
}

Expected<DocComment> APIJSONParser::parseDocComment(const Object *obj) {
  const Object *docCommentObject = obj->getObject("docComment");
  DocComment docComment;
  if (!docCommentObject)
    return docComment;

  const Array *lines = docCommentObject->getArray("lines");
  if (!lines)
    return make_error<APIJSONError>("Missing 'lines' in docComment object");

  for (const auto &line : *lines) {
    const Object *lineObject = line.getAsObject();
    if (!lineObject)
      return make_error<APIJSONError>("Expect to be JSON Object");

    auto text =
        parseString("text", lineObject, "Missing 'text' in docComment line");
    if (!text)
      return text.takeError();

    const Object *rangeObject = lineObject->getObject("range");
    if (!rangeObject)
      return make_error<APIJSONError>("Missing 'range' in docComment line");

    const StringRef file = obj->getString("file").getValueOr("");

    auto range = parseRange(rangeObject, file);
    if (!range)
      return range.takeError();

    docComment.addCommentLine(*text, *range);
  }

  return docComment;
}

Expected<DeclarationFragments>
APIJSONParser::parseDeclarationFragmentsFrom(const Array *array) {
  DeclarationFragments fragments;

  for (const auto &fragment : *array) {
    const Object *fragmentObject = fragment.getAsObject();
    if (!fragmentObject)
      return make_error<APIJSONError>("Expect to be JSON Object");

    auto spelling = parseString("spelling", fragmentObject,
                                "Missing 'spelling' in declaration fragment");
    if (!spelling)
      return spelling.takeError();

    auto kind = parseString("kind", fragmentObject,
                            "Missing 'kind' in declaration fragment");
    if (!kind)
      return kind.takeError();

    auto preciseIdentifier =
        fragmentObject->getString("preciseIdentifier").getValueOr("");

    fragments.append(
        *spelling, DeclarationFragments::parseFragmentKindFromString(*kind),
        preciseIdentifier);
  }

  return fragments;
}

Expected<DeclarationFragments>
APIJSONParser::parseDeclarationFragments(const Object *obj) {
  const Array *declFragmentsArray = obj->getArray("declarationFragments");
  if (!declFragmentsArray)
    return DeclarationFragments{};

  return parseDeclarationFragmentsFrom(declFragmentsArray);
}

Expected<FunctionSignature>
APIJSONParser::parseFunctionSignature(const Object *obj) {
  FunctionSignature signature;
  const Object *sigObj = obj->getObject("functionSignature");
  if (!sigObj)
    return signature;

  if (const Array *returnType = sigObj->getArray("returns")) {
    auto returnFragments = parseDeclarationFragmentsFrom(returnType);
    if (!returnFragments)
      return returnFragments.takeError();
    signature.setReturnType(*returnFragments);
  } else {
    return make_error<APIJSONError>("Expect to be a JSON Array");
  }

  if (const Array *params = sigObj->getArray("parameters")) {
    for (const auto &param : *params) {
      const Object *paramObj = param.getAsObject();
      if (!paramObj)
        return make_error<APIJSONError>("Expect to be a JSON Object");

      auto name = parseString("name", paramObj,
          "Missing 'name' in function signature parameter");

      if (!name)
        return name.takeError();

      const Array *paramFragments = paramObj->getArray("declarationFragments");

      if (!paramFragments)
        return make_error<APIJSONError>("Missing 'declarationFragments' in function signature parameter");

      auto fragments = parseDeclarationFragmentsFrom(paramFragments);
      if (!fragments)
        return fragments.takeError();

      signature.addParameter(*name, *fragments);
    }
  } else {
    return make_error<APIJSONError>("Expect to be a JSON Array");
  }

  return signature;
}

Expected<DeclarationFragments>
APIJSONParser::parseSubHeading(const Object *obj) {
  if (const Array *fragments= obj->getArray("subHeading"))
    return parseDeclarationFragmentsFrom(fragments);
  return DeclarationFragments();
}

Error APIJSONParser::parseMacros(Array &macros) {
  for (const auto &macro : macros) {
    auto *object = macro.getAsObject();
    if (!object)
      return make_error<APIJSONError>("Expect to be JSON Object");

    auto name = parseName(object);
    if (!name)
      return name.takeError();
    auto access = parseAccess(object);
    if (!access)
      return access.takeError();
    auto loc = parseLocation(object);
    if (!loc)
      return loc.takeError();
    auto usr = parseUSR(object);

    auto declarationFragments = parseDeclarationFragments(object);
    if (!declarationFragments)
      return declarationFragments.takeError();

    result.addMacroDefinition(*name, usr, *loc, *access, *declarationFragments);
  }

  return Error::success();
}

Error APIJSONParser::parseGlobals(Array &globals) {
  for (const auto &g : globals) {
    auto *object = g.getAsObject();
    if (!object)
      return make_error<APIJSONError>("Expect to be JSON Object");

    auto name = parseName(object);
    if (!name)
      return name.takeError();
    auto declName = parseDeclName(object);
    auto access = parseAccess(object);
    if (!access)
      return access.takeError();
    auto kind = parseGlobalKind(object);
    if (!kind)
      return kind.takeError();
    auto linkage = parseLinkage(object);
    if (!linkage)
      return linkage.takeError();
    auto loc = parseLocation(object);
    if (!loc)
      return loc.takeError();
    auto avail = parseAvailability(object);
    if (!avail)
      return avail.takeError();
    auto flags = parseFlags(object);
    auto usr = parseUSR(object);
    auto docComment = parseDocComment(object);
    if (!docComment)
      return docComment.takeError();
    auto declarationFragments = parseDeclarationFragments(object);
    if (!declarationFragments)
      return declarationFragments.takeError();
    auto subHeading = parseSubHeading(object);
    if (!subHeading)
      return subHeading.takeError();
    auto functionSignature = parseFunctionSignature(object);
    if (!functionSignature)
      return functionSignature.takeError();

    result.addGlobal(*name, declName, usr, flags, *loc, *avail, *access,
                     *docComment, *declarationFragments, *subHeading,
                     *functionSignature,
                     /*Decl*/ nullptr, *kind, *linkage);
  }

  return Error::success();
}

SymbolInfo APIJSONParser::parseSymbolInfo(const Value *value) {
  SymbolInfo symbol;
  if (const Object *obj = value->getAsObject()) {
    symbol.name = result.copyString(obj->getString("name").getValueOr(""));
    symbol.usr = result.copyString(parseUSR(obj));
    symbol.sourceModule =
        result.copyString(obj->getString("sourceModule").getValueOr(""));
  } else
    symbol.name = result.copyString(value->getAsString().getValueOr(""));

  return symbol;
}

SymbolInfo APIJSONParser::parseSuperClass(const Object *obj) {
  SymbolInfo super;
  super.name = result.copyString(obj->getString("super").getValueOr(""));
  super.usr = result.copyString(obj->getString("superUSR").getValueOr(""));
  super.sourceModule =
      result.copyString(obj->getString("superSourceModule").getValueOr(""));

  return super;
}

SymbolInfo APIJSONParser::parseCategoryInterface(const Object *obj) {
  SymbolInfo interface;
  interface.name =
      result.copyString(obj->getString("interface").getValueOr(""));
  interface.usr =
      result.copyString(obj->getString("interfaceUSR").getValueOr(""));
  interface.sourceModule =
      result.copyString(obj->getString("interfaceSourceModule").getValueOr(""));

  return interface;
}

SymbolInfo APIJSONParser::parseUnderlyingType(const Object *obj) {
  SymbolInfo type;
  type.name = result.copyString(obj->getString("type").getValueOr(""));
  type.usr = result.copyString(obj->getString("typeUSR").getValueOr(""));
  type.sourceModule =
      result.copyString(obj->getString("typeSourceModule").getValueOr(""));

  return type;
}

Error APIJSONParser::parseConformedProtocols(ObjCContainerRecord *container,
                                             const Object *obj) {
  const auto *protocols = obj->getArray("protocols");
  if (!protocols)
    return Error::success();

  for (const auto &p : *protocols)
    container->protocols.emplace_back(parseSymbolInfo(&p));

  return Error::success();
}

Error APIJSONParser::parseMethods(ObjCContainerRecord *container,
                                  const Object *object, bool isInstanceMethod) {
  const auto *key = isInstanceMethod ? "instanceMethods" : "classMethods";
  const auto *methods = object->getArray(key);
  if (!methods)
    return Error::success();

  for (const auto &m : *methods) {
    const auto *method = m.getAsObject();
    if (!method)
      return make_error<APIJSONError>("method should be an object");
    auto name = parseName(method);
    if (!name)
      return name.takeError();
    auto access = parseAccess(method);
    if (!access)
      return access.takeError();
    auto loc = parseLocation(method);
    if (!loc)
      return loc.takeError();
    auto avail = parseAvailability(method);
    if (!avail)
      return avail.takeError();
    auto isOptional = parseBinaryField("optional", method);
    auto isDynamic = parseBinaryField("dynamic", method);
    auto usr = parseUSR(method);
    auto docComment = parseDocComment(method);
    if (!docComment)
      return docComment.takeError();
    auto declarationFragments = parseDeclarationFragments(method);
    if (!declarationFragments)
      return declarationFragments.takeError();
    auto subHeading = parseSubHeading(object);
    if (!subHeading)
      return subHeading.takeError();
    auto functionSignature = parseFunctionSignature(method);
    if (!functionSignature)
      return functionSignature.takeError();

    result.addObjCMethod(container, *name, usr, *loc, *avail, *access,
                         isInstanceMethod, isOptional, isDynamic, *docComment,
                         *declarationFragments, *subHeading, *functionSignature,
                         /*Decl*/ nullptr);
  }

  return Error::success();
}

Error APIJSONParser::parseProperties(ObjCContainerRecord *container,
                                     const Object *object) {
  const auto *properties = object->getArray("properties");
  if (!properties)
    return Error::success();

  for (const auto &p : *properties) {
    const auto *property = p.getAsObject();
    if (!property)
      return make_error<APIJSONError>("property should be an object");
    auto name = parseName(property);
    if (!name)
      return name.takeError();
    auto access = parseAccess(property);
    if (!access)
      return access.takeError();
    auto loc = parseLocation(property);
    if (!loc)
      return loc.takeError();
    auto avail = parseAvailability(property);
    if (!avail)
      return avail.takeError();
    auto attr = parsePropertyAttribute(property);
    if (!attr)
      return attr.takeError();
    auto getter = parseString("getter", property, "cannot find getter");
    if (!getter)
      return getter.takeError();

    StringRef setter;
    if (!(*attr & PAKind::ReadOnly)) {
      auto setterStr = parseString("setter", property, "cannot find setter");
      if (!setterStr)
        return setterStr.takeError();
      setter = *setterStr;
    }
    auto isOptional = parseBinaryField("optional", property);
    auto usr = parseUSR(property);
    auto docComment = parseDocComment(property);
    if (!docComment)
      return docComment.takeError();
    auto declarationFragments = parseDeclarationFragments(property);
    if (!declarationFragments)
      return declarationFragments.takeError();
    auto subHeading = parseSubHeading(object);
    if (!subHeading)
      return subHeading.takeError();

    result.addObjCProperty(container, *name, usr, *getter, setter, *loc, *avail,
                           *access, *attr, isOptional, *docComment,
                           *declarationFragments, *subHeading,
                           /*Decl*/ nullptr);
  }

  return Error::success();
}

Error APIJSONParser::parseIvars(ObjCContainerRecord *container,
                                const Object *object) {
  const auto *ivars = object->getArray("ivars");
  if (!ivars)
    return Error::success();

  for (const auto &i : *ivars) {
    const auto *ivar = i.getAsObject();
    if (!ivar)
      return make_error<APIJSONError>("ivar should be an object");
    auto name = parseName(ivar);
    if (!name)
      return name.takeError();
    auto access = parseAccess(ivar);
    if (!access)
      return access.takeError();
    auto loc = parseLocation(ivar);
    if (!loc)
      return loc.takeError();
    auto avail = parseAvailability(ivar);
    if (!avail)
      return avail.takeError();
    auto control = parseAccessControl(ivar);
    if (!control)
      return control.takeError();
    auto linkage = parseLinkage(ivar);
    if (!linkage)
      return linkage.takeError();
    auto usr = parseUSR(ivar);
    auto docComment = parseDocComment(ivar);
    if (!docComment)
      return docComment.takeError();
    auto declarationFragments = parseDeclarationFragments(ivar);
    if (!declarationFragments)
      return declarationFragments.takeError();
    auto subHeading = parseSubHeading(object);
    if (!subHeading)
      return subHeading.takeError();

    result.addObjCInstanceVariable(container, *name, usr, *loc, *avail, *access,
                                   *control, *linkage, *docComment,
                                   *declarationFragments, *subHeading,
                                   /*Decl*/ nullptr);
  }
  return Error::success();
}

Error APIJSONParser::parseInterfaces(Array &interfaces) {
  for (const auto &interface : interfaces) {
    auto *object = interface.getAsObject();
    if (!object)
      return make_error<APIJSONError>("Expect to be JSON Object");

    auto name = parseName(object);
    if (!name)
      return name.takeError();
    auto declName = parseDeclName(object);
    auto access = parseAccess(object);
    if (!access)
      return access.takeError();
    auto loc = parseLocation(object);
    if (!loc)
      return loc.takeError();
    auto avail = parseAvailability(object);
    if (!avail)
      return avail.takeError();
    auto linkage = parseLinkage(object);
    if (!linkage)
      return linkage.takeError();
    auto usr = parseUSR(object);
    auto docComment = parseDocComment(object);
    if (!docComment)
      return docComment.takeError();
    auto declarationFragments = parseDeclarationFragments(object);
    if (!declarationFragments)
      return declarationFragments.takeError();
    auto subHeading = parseSubHeading(object);
    if (!subHeading)
      return subHeading.takeError();
    SymbolInfo super = parseSuperClass(object);

    auto *objcClass = result.addObjCInterface(
        *name, declName, usr, *loc, *avail, *access, *linkage, super,
        *docComment, *declarationFragments, *subHeading,
        /*Decl*/ nullptr);
    auto exception = parseBinaryField("hasException", object);
    objcClass->hasExceptionAttribute = exception;

    // Don't need to handle categories here.
    auto err = parseConformedProtocols(objcClass, object);
    if (err)
      return err;

    err = parseMethods(objcClass, object, true);
    if (err)
      return err;

    err = parseMethods(objcClass, object, false);
    if (err)
      return err;

    err = parseProperties(objcClass, object);
    if (err)
      return err;

    err = parseIvars(objcClass, object);
    if (err)
      return err;
  }
  return Error::success();
}

Error APIJSONParser::parseCategories(Array &categories) {
  for (const auto &category : categories) {
    auto *object = category.getAsObject();
    if (!object)
      return make_error<APIJSONError>("Expect to be JSON Object");

    auto name = parseName(object);
    if (!name)
      return name.takeError();
    auto access = parseAccess(object);
    if (!access)
      return access.takeError();
    auto loc = parseLocation(object);
    if (!loc)
      return loc.takeError();
    auto avail = parseAvailability(object);
    if (!avail)
      return avail.takeError();
    auto usr = parseUSR(object);
    auto docComment = parseDocComment(object);
    if (!docComment)
      return docComment.takeError();
    auto declarationFragments = parseDeclarationFragments(object);
    if (!declarationFragments)
      return declarationFragments.takeError();
    auto subHeading = parseSubHeading(object);
    if (!subHeading)
      return subHeading.takeError();
    SymbolInfo interface = parseCategoryInterface(object);

    auto *objcCategory = result.addObjCCategory(
        interface, *name, usr, *loc, *avail, *access, *docComment,
        *declarationFragments, *subHeading, /*Decl*/ nullptr);

    // Don't need to handle categories here.
    auto err = parseConformedProtocols(objcCategory, object);
    if (err)
      return err;

    err = parseMethods(objcCategory, object, true);
    if (err)
      return err;

    err = parseMethods(objcCategory, object, false);
    if (err)
      return err;

    err = parseProperties(objcCategory, object);
    if (err)
      return err;

    err = parseIvars(objcCategory, object);
    if (err)
      return err;
  }
  return Error::success();
}

Error APIJSONParser::parseProtocols(Array &protocols) {
  for (const auto &protocol : protocols) {
    auto *object = protocol.getAsObject();
    if (!object)
      return make_error<APIJSONError>("Expect to be JSON Object");

    auto name = parseName(object);
    if (!name)
      return name.takeError();
    auto access = parseAccess(object);
    if (!access)
      return access.takeError();
    auto loc = parseLocation(object);
    if (!loc)
      return loc.takeError();
    auto avail = parseAvailability(object);
    if (!avail)
      return avail.takeError();
    auto usr = parseUSR(object);
    auto docComment = parseDocComment(object);
    if (!docComment)
      return docComment.takeError();
    auto declarationFragments = parseDeclarationFragments(object);
    if (!declarationFragments)
      return declarationFragments.takeError();
    auto subHeading = parseSubHeading(object);
    if (!subHeading)
      return subHeading.takeError();

    auto *objcProtocol = result.addObjCProtocol(
        *name, usr, *loc, *avail, *access, *docComment, *declarationFragments,
        *subHeading, /*Decl*/ nullptr);

    // Don't need to handle categories here.
    auto err = parseConformedProtocols(objcProtocol, object);
    if (err)
      return err;

    err = parseMethods(objcProtocol, object, true);
    if (err)
      return err;

    err = parseMethods(objcProtocol, object, false);
    if (err)
      return err;

    err = parseProperties(objcProtocol, object);
    if (err)
      return err;

    err = parseIvars(objcProtocol, object);
    if (err)
      return err;
  }
  return Error::success();
}

Error APIJSONParser::parseEnumConstants(EnumRecord *record,
                                        const Object *object) {
  const auto *constants = object->getArray("constants");
  if (!constants)
    return Error::success();

  for (const auto &c : *constants) {
    const auto *constant = c.getAsObject();
    if (!constant)
      return make_error<APIJSONError>("enum constant should be an object");
    auto name = parseName(constant);
    if (!name)
      return name.takeError();
    auto declName = parseDeclName(constant);
    auto access = parseAccess(constant);
    if (!access)
      return access.takeError();
    auto loc = parseLocation(constant);
    if (!loc)
      return loc.takeError();
    auto avail = parseAvailability(constant);
    if (!avail)
      return avail.takeError();
    auto usr = parseUSR(constant);
    auto docComment = parseDocComment(constant);
    if (!docComment)
      return docComment.takeError();
    auto declarationFragments = parseDeclarationFragments(constant);
    if (!declarationFragments)
      return declarationFragments.takeError();
    auto subHeading = parseSubHeading(object);
    if (!subHeading)
      return subHeading.takeError();

    result.addEnumConstant(record, *name, declName, usr, *loc, *avail, *access,
                           *docComment, *declarationFragments, *subHeading,
                           /*Decl*/ nullptr);
  }

  return Error::success();
}

Error APIJSONParser::parseEnums(Array &enums) {
  for (const auto &e : enums) {
    auto *object = e.getAsObject();
    if (!object)
      return make_error<APIJSONError>("Expect to be JSON Object");

    auto name = parseName(object);
    if (!name)
      return name.takeError();
    auto declName = parseDeclName(object);
    auto access = parseAccess(object);
    if (!access)
      return access.takeError();
    auto loc = parseLocation(object);
    if (!loc)
      return loc.takeError();
    auto avail = parseAvailability(object);
    if (!avail)
      return avail.takeError();
    auto usr = parseUSR(object);
    auto docComment = parseDocComment(object);
    if (!docComment)
      return docComment.takeError();
    auto declarationFragments = parseDeclarationFragments(object);
    if (!declarationFragments)
      return declarationFragments.takeError();
    auto subHeading = parseSubHeading(object);
    if (!subHeading)
      return subHeading.takeError();

    auto *record =
        result.addEnum(*name, declName, usr, *loc, *avail, *access, *docComment,
                       *declarationFragments, *subHeading,
                       /*Decl*/ nullptr);
    auto err = parseEnumConstants(record, object);
    if (err)
      return err;
  }
  return Error::success();
}

Error APIJSONParser::parseTypedefs(Array &typedefs) {
  for (const auto &type : typedefs) {
    auto *object = type.getAsObject();
    if (!object)
      return make_error<APIJSONError>("Expect to be JSON Object");

    auto name = parseName(object);
    if (!name)
      return name.takeError();
    auto access = parseAccess(object);
    if (!access)
      return access.takeError();
    auto loc = parseLocation(object);
    if (!loc)
      return loc.takeError();
    auto avail = parseAvailability(object);
    if (!avail)
      return avail.takeError();
    auto usr = parseUSR(object);
    auto docComment = parseDocComment(object);
    if (!docComment)
      return docComment.takeError();
    auto declarationFragments = parseDeclarationFragments(object);
    if (!declarationFragments)
      return declarationFragments.takeError();
    auto subHeading = parseSubHeading(object);
    if (!subHeading)
      return subHeading.takeError();
    SymbolInfo underlyingType = parseUnderlyingType(object);

    result.addTypeDef(*name, usr, *loc, *avail, *access, underlyingType,
                      *docComment, *declarationFragments, *subHeading,
                      /*Decl*/ nullptr);
  }
  return Error::success();
}

Error APIJSONParser::parseStructFields(StructRecord *record,
                                       const Object *object) {
  const auto *fields = object->getArray("fields");
  if (!fields)
    return Error::success();

  for (const auto &f : *fields) {
    const auto *field = f.getAsObject();
    if (!field)
      return make_error<APIJSONError>("struct field should be an object");
    auto name = parseName(field);
    if (!name)
      return name.takeError();
    auto declName = parseDeclName(field);
    auto access = parseAccess(field);
    if (!access)
      return access.takeError();
    auto loc = parseLocation(field);
    if (!loc)
      return loc.takeError();
    auto avail = parseAvailability(field);
    if (!avail)
      return avail.takeError();
    auto usr = parseUSR(field);
    auto docComment = parseDocComment(field);
    if (!docComment)
      return docComment.takeError();
    auto declarationFragments = parseDeclarationFragments(object);
    if (!declarationFragments)
      return declarationFragments.takeError();
    auto subHeading = parseSubHeading(object);
    if (!subHeading)
      return subHeading.takeError();

    result.addStructField(record, *name, declName, usr, *loc, *avail, *access,
                          *docComment, *declarationFragments, *subHeading,
                          /*Decl*/ nullptr);
  }

  return Error::success();
}

Error APIJSONParser::parseStructs(Array &structs) {
  for (const auto &s : structs) {
    auto *object = s.getAsObject();
    if (!object)
      return make_error<APIJSONError>("Expect to be JSON Object");

    auto name = parseName(object);
    if (!name)
      return name.takeError();
    auto access = parseAccess(object);
    if (!access)
      return access.takeError();
    auto loc = parseLocation(object);
    if (!loc)
      return loc.takeError();
    auto avail = parseAvailability(object);
    if (!avail)
      return avail.takeError();
    auto usr = parseUSR(object);
    auto docComment = parseDocComment(object);
    if (!docComment)
      return docComment.takeError();
    auto declarationFragments = parseDeclarationFragments(object);
    if (!declarationFragments)
      return declarationFragments.takeError();
    auto subHeading = parseSubHeading(object);
    if (!subHeading)
      return subHeading.takeError();

    auto *record =
        result.addStruct(*name, usr, *loc, *avail, *access, *docComment,
                         *declarationFragments, *subHeading, /*Decl*/ nullptr);
    auto err = parseStructFields(record, object);
    if (err)
      return err;
  }
  return Error::success();
}

Error APIJSONParser::parseBinaryInfo(Object &binaryInfo) {
  auto &info = result.getBinaryInfo();
  auto fileType = binaryInfo.getString("type");
  if (!fileType)
    return make_error<APIJSONError>("expected filetype");
  if (*fileType == "dylib")
    info.fileType = FileType::MachO_DynamicLibrary;
  else if (*fileType == "stub")
    info.fileType = FileType::MachO_DynamicLibrary_Stub;
  else if (*fileType == "bundle")
    info.fileType = FileType::MachO_Bundle;
  else if (*fileType == "invalid")
    info.fileType = FileType::Invalid;
  else
    return make_error<APIJSONError>("un-expected filetype: " + *fileType);

  auto currentVersion = binaryInfo.getString("currentVersion");
  if (!currentVersion)
    return make_error<APIJSONError>("expected currentVersion");
  info.currentVersion.parse32(*currentVersion);
  auto compatibilityVersion = binaryInfo.getString("compatibilityVersion");
  if (!compatibilityVersion)
    return make_error<APIJSONError>("expected compatibilityVersion");
  info.compatibilityVersion.parse32(*compatibilityVersion);

  auto installName = binaryInfo.getString("installName");
  if (!installName)
    return make_error<APIJSONError>("missing install name");
  info.installName = result.copyString(*installName);
  auto uuid = binaryInfo.getString("uuid");
  if (uuid)
    info.uuid = result.copyString(*uuid);

  if (auto parentUmbrella = binaryInfo.getString("parentUmbrella"))
    info.parentUmbrella = result.copyString(*parentUmbrella);
  info.isTwoLevelNamespace = parseBinaryField("twoLevelNamespace", &binaryInfo);
  info.isAppExtensionSafe = parseBinaryField("appExtensionSafe", &binaryInfo);

  if (auto *allowableClients = binaryInfo.getArray("allowableClients")) {
    for (const auto &client : *allowableClients) {
      auto name = client.getAsString();
      if (!name)
        return make_error<APIJSONError>("allowableClient is not string");
      info.allowableClients.emplace_back(result.copyString(*name));
    }
  }
  if (auto *reexportedLibraries = binaryInfo.getArray("reexportedLibraries")) {
    for (const auto &lib : *reexportedLibraries) {
      auto name = lib.getAsString();
      if (!name)
        return make_error<APIJSONError>("reexportedLibrary is not string");
      info.reexportedLibraries.emplace_back(result.copyString(*name));
    }
  }
  return Error::success();
}

Error APIJSONParser::parsePotentiallyDefinedSelectors(Array &selectors) {
  for (auto &s : selectors) {
    auto name = s.getAsString();
    if (!name)
      return make_error<APIJSONError>(
          "potentially defined selector is not string");
    result.getPotentiallyDefinedSelectors().insert(*name);
  }
  return Error::success();
}

Error APIJSONParser::parse(Object *root) {
  auto *macros = root->getArray("macros");
  if (macros) {
    auto err = parseMacros(*macros);
    if (err)
      return err;
  }

  auto *globals = root->getArray("globals");
  if (globals) {
    auto err = parseGlobals(*globals);
    if (err)
      return err;
  }

  auto *protocols = root->getArray("protocols");
  if (protocols) {
    auto err = parseProtocols(*protocols);
    if (err)
      return err;
  }

  auto *interfaces = root->getArray("interfaces");
  if (interfaces) {
    auto err = parseInterfaces(*interfaces);
    if (err)
      return err;
  }

  auto *categories = root->getArray("categories");
  if (categories) {
    auto err = parseCategories(*categories);
    if (err)
      return err;
  }

  auto *enums = root->getArray("enums");
  if (enums) {
    auto err = parseEnums(*enums);
    if (err)
      return err;
  }

  auto *typedefs = root->getArray("typedefs");
  if (typedefs) {
    auto err = parseTypedefs(*typedefs);
    if (err)
      return err;
  }

  auto *structs = root->getArray("structs");
  if (structs) {
    auto err = parseStructs(*structs);
    if (err)
      return err;
  }

  auto *selectors = root->getArray("potentiallyDefinedSelectors");
  if (selectors) {
    auto err = parsePotentiallyDefinedSelectors(*selectors);
    if (err)
      return err;
  }

  auto *binaryInfo = root->getObject("binaryInfo");
  if (binaryInfo) {
    auto err = parseBinaryInfo(*binaryInfo);
    if (err)
      return err;
  }

  return Error::success();
}

Expected<API> APIJSONSerializer::parse(StringRef json) {
  auto inputValue = json::parse(json);
  if (!inputValue)
    return inputValue.takeError();

  auto *root = inputValue->getAsObject();
  if (!root)
    return make_error<APIJSONError>("API is not a JSON Object");

  auto version = root->getInteger("api_json_version");
  if (!version || *version != 1)
    return make_error<APIJSONError>("Input JSON has unsupported version");

  return parse(root);
}

Expected<API> APIJSONSerializer::parse(Object *root, bool publicOnly,
                                       Triple *triple) {
  auto target = root->getString("target");
  std::string targetStr;
  if (target)
    targetStr = target->str();
  else if (triple)
    targetStr = triple->normalize();
  else
    return make_error<APIJSONError>("Input triple is not expected");

  Triple targetTriples(targetStr);
  API result(targetTriples);

  auto project = root->getString("project");
  if (project)
    result.setProjectName(*project);

  APIJSONParser parser(result, publicOnly);
  auto err = parser.parse(root);
  if (err)
    return std::move(err);

  return result;
}

TAPI_NAMESPACE_INTERNAL_END
