//===- tapi/SDKDB/SDKDB.cpp - TAPI SDKDB ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Implements the SDKDB
///
//===----------------------------------------------------------------------===//

#include "tapi/SDKDB/SDKDB.h"

#include "tapi/Core/APIJSONSerializer.h"
#include "tapi/Core/APIVisitor.h"
#include "tapi/Core/Utils.h"
#include "tapi/Diagnostics/Diagnostics.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/JSON.h"
#include <vector>

using namespace llvm;

TAPI_NAMESPACE_INTERNAL_BEGIN

class LookupMapBuilder : public APIMutator {
public:
  LookupMapBuilder(SDKDB &sdkdb, API &api) : sdkdb(sdkdb), api(api) {
    binInfo = api.hasBinaryInfo() ? &api.getBinaryInfo() : nullptr;
  }

  void visitGlobal(GlobalRecord &record) override {
    sdkdb.insertGlobal(&record, binInfo, api.getProjectName());
  }
  void visitObjCInterface(ObjCInterfaceRecord &record) override {
    sdkdb.insertObjCInterface(&record, binInfo, api.getProjectName());
  }
  void visitObjCCategory(ObjCCategoryRecord &record) override {
    sdkdb.insertObjCCategory(&record, binInfo, api.getProjectName());
  }
  void visitObjCProtocol(ObjCProtocolRecord &record) override {
    sdkdb.insertObjCProtocol(&record, binInfo, api.getProjectName());
  }
  void visitEnum(EnumRecord &record) override {
    sdkdb.insertEnum(&record, binInfo, api.getProjectName());
  }
  void visitTypeDef(TypedefRecord &record) override {
    sdkdb.insertTypeDef(&record, binInfo, api.getProjectName());
  }

private:
  SDKDB &sdkdb;
  API &api;
  const BinaryInfo *binInfo;
};

class APIAnnotator : public APIVisitor {
public:
  APIAnnotator(SDKDB &sdkdb, StringRef project)
      : sdkdb(sdkdb), project(project) {}

  void visitGlobal(const GlobalRecord &record) override {
    sdkdb.annotateGlobal(&record);
  }
  void visitObjCInterface(const ObjCInterfaceRecord &record) override {
    sdkdb.annotateObjCInterface(&record);
  }
  void visitObjCCategory(const ObjCCategoryRecord &record) override {
    sdkdb.annotateObjCCategory(&record);
  }
  void visitObjCProtocol(const ObjCProtocolRecord &record) override {
    sdkdb.annotateObjCProtocol(&record);
  }
  void visitEnum(const EnumRecord &record) override {
    // Make a copy of the record in frontendAPI.
    // enums are in the HeaderAPIs which we do not preserve in SDKDB.
    auto *copy = sdkdb.addEnum(record);
    sdkdb.insertEnum(copy, nullptr, project);
  }

  void visitTypeDef(const TypedefRecord &record) override {
    // create a copy because typedefs are only in header APIs.
    auto *copy = sdkdb.addTypeDef(record);
    sdkdb.insertTypeDef(copy, nullptr, project);
  }

private:
  SDKDB &sdkdb;
  StringRef project;
};

bool SDKDB::areCompatibleTargets(const Triple &lhs, const Triple &rhs) {
  if (lhs != rhs)
    return false;

  // x86_64 and x86_64h are two different slices.
  if (lhs.getArchName().startswith("x86_64") &&
      lhs.getArchName() != rhs.getArchName())
    return false;

  return true;
}

API &SDKDB::recordAPI(API &&api) {
  auto name = api.getProjectName().str();
  apiCache[name].emplace_back(std::move(api));
  return apiCache[name].back();
}

void SDKDB::insertGlobal(GlobalRecord *record, const BinaryInfo *binInfo,
                         StringRef project) {
  // only insert exported and re-exported symbols into map.
  if (record->linkage < APILinkage::Reexported)
    return;

  auto key = record->name;
  auto &value = globalMap[key];

  // Check for duplicated non weak symbols.
  if (!value.empty() && !record->isWeakDefined()) {
    if (any_of(value, [](const MapEntry<GlobalRecord *> &entry) {
          return !entry.getRecord()->isWeakDefined();
        }))
      builder->report(diag::warn_sdkdb_duplicated_global) << key;
  }

  value.emplace_back(record, binInfo, project);
}

void SDKDB::insertObjCInterface(ObjCInterfaceRecord *record,
                                const BinaryInfo *binInfo, StringRef project) {
  auto key = record->name;
  auto result = interfaceMap.try_emplace(key, record, binInfo, project);
  // If emplace successful.
  if (result.second)
    return; 

  auto entry = result.first;
  // Entry has been seen before, report duplicated class.
  builder->report(diag::warn_sdkdb_duplicated_objc_class) << key;

  // If entires are "equal", set the entry to poison since we don't know which
  // to pick so we pick neither.
  MapEntry<ObjCInterfaceRecord*> current{record, binInfo, project};
  if (current == entry->getValue()) {
    entry->getValue().setPoison();
    return;
  }

  // Set the value if current value should replace the one in the map.
  if (current < entry->getValue())
    entry->setValue(current);
}

void SDKDB::insertObjCCategory(ObjCCategoryRecord *record,
                               const BinaryInfo *binInfo, StringRef project) {
  auto &catMap = categoryMap[record->interface.name];
  auto result = catMap.try_emplace(record->name, record, binInfo, project);
  // If emplace successful.
  if (result.second)
    return; 

  auto entry = result.first;
  builder->report(diag::warn_sdkdb_duplicated_objc_category)
      << record->interface.name << record->name;

  // If entires are "equal", set the entry to poison since we don't know which
  // to pick so we pick neither.
  MapEntry<ObjCCategoryRecord*> current{record, binInfo, project};
  if (current == entry->getValue()) {
    entry->getValue().setPoison();
    return;
  }

  // Set the value if current value should replace the one in the map.
  if (current < entry->getValue())
    entry->setValue(current);
}

EnumRecord *SDKDB::addEnum(const EnumRecord &record) {
  auto *copy = frontendAPI.addEnum(
      record.name, record.declName, record.usr, record.loc, record.availability,
      record.access, record.docComment, record.declarationFragments,
      record.subHeading, record.decl);
  for (auto *constant : record.constants)
    frontendAPI.addEnumConstant(
        copy, constant->name, constant->declName, constant->usr, constant->loc,
        constant->availability, constant->access, constant->docComment,
        constant->declarationFragments, constant->subHeading, constant->decl);

  return copy;
}

TypedefRecord *SDKDB::addTypeDef(const TypedefRecord &record) {
  return frontendAPI.addTypeDef(
      record.name, record.usr, record.loc, record.availability, record.access,
      record.underlyingType, record.docComment, record.declarationFragments,
      record.subHeading, record.decl);
}

void SDKDB::insertEnum(EnumRecord *record, const BinaryInfo *binInfo,
                       StringRef project) {
  auto key = record->name;
  auto result = enumMap.try_emplace(key, record, binInfo, project);
  // If emplace successful.
  if (result.second)
    return;

  auto entry = result.first;
  // Entry has been seen before, report duplicated enum.
  builder->report(diag::warn_sdkdb_duplicated_enum) << key;

  MapEntry<EnumRecord*> current{record, binInfo, project};
  if (current == entry->getValue()) {
    entry->getValue().setPoison();
    return;
  }

  if (current < entry->getValue())
    entry->setValue(current);
}

void SDKDB::insertTypeDef(TypedefRecord *record, const BinaryInfo *binInfo,
                          StringRef project) {
  auto key = record->name;
  auto result = typedefMap.try_emplace(key, record, binInfo, project);
  // If emplace successful.
  if (result.second)
    return;

  auto entry = result.first;
  // Entry has been seen before, report duplicated enum.
  builder->report(diag::warn_sdkdb_duplicated_typedef) << key;

  MapEntry<TypedefRecord *> current{record, binInfo, project};
  if (current == entry->getValue()) {
    entry->getValue().setPoison();
    return;
  }

  if (current < entry->getValue())
    entry->setValue(current);
}

SmallVector<GlobalRecord *, 2> SDKDB::findGlobals(StringRef name) const {
  SmallVector<GlobalRecord *, 2> globals;
  auto global = globalMap.find(name);
  if (global != globalMap.end()) {
    for (auto &g : global->getValue())
      globals.emplace_back(g.getRecord());
  }
  return globals;
}

ObjCInterfaceRecord *SDKDB::findObjCInterface(StringRef name) const {
  auto cls = interfaceMap.find(name);
  if (cls == interfaceMap.end() || cls->getValue().isPoison())
    return nullptr;

  return cls->getValue().getRecord();
}

ObjCCategoryRecord *SDKDB::findObjCCategory(StringRef categoryName,
                                            StringRef clsName) const {
  auto categories = categoryMap.find(clsName);
  if (categories == categoryMap.end())
    return nullptr;

  auto category = categories->getValue().find(categoryName);
  if (category == categories->getValue().end() ||
      category->getValue().isPoison())
    return nullptr;

  return category->getValue().getRecord();
}

EnumRecord *SDKDB::findEnum(StringRef name) const {
  auto e = enumMap.find(name);
  if (e == enumMap.end() || e->getValue().isPoison())
    return nullptr;

  return e->getValue().getRecord();
}

APIRecord *SDKDB::findTypedef(StringRef name) const {
  auto t = typedefMap.find(name);
  if (t == typedefMap.end() || t->getValue().isPoison())
    return nullptr;

  return t->getValue().getRecord();
}

SmallVector<ObjCCategoryRecord *, 4>
SDKDB::findObjCCategoryForClass(StringRef clsName) const {
  SmallVector<ObjCCategoryRecord *, 4> categories;

  auto entry = categoryMap.find(clsName);
  if (entry == categoryMap.end())
    return categories;

  for (auto &c : entry->getValue()) {
    if (c.getValue().isPoison())
      continue;
    categories.emplace_back(c.getValue().getRecord());
  }

  // Sort by category names.
  llvm::sort(categories,
             [](const ObjCCategoryRecord *lhs, const ObjCCategoryRecord *rhs) {
               return lhs->name < rhs->name;
             });

  return categories;
}

ObjCProtocolRecord *SDKDB::findObjCProtocol(StringRef protocolName) const {
  auto protocol = protocolMap.find(protocolName);
  if (protocol == protocolMap.end() || protocol->getValue().isPoison())
    return nullptr;

  return protocol->getValue().getRecord();
}

static ObjCMethodRecord *findMethodFromContainer(StringRef name,
                                                 bool isInstanceMethod,
                                                 ObjCContainerRecord &record) {
  auto result = find_if(record.methods, [&](ObjCMethodRecord *method) {
    return method->name == name && method->isInstanceMethod == isInstanceMethod;
  });
  if (result == record.methods.end())
    return nullptr;

  return *result;
}

static ObjCPropertyRecord *findProperty(StringRef name,
                                        ObjCContainerRecord &record) {
  auto result = find_if(record.properties, [&](ObjCPropertyRecord *property) {
    return property->name == name;
  });
  if (result == record.properties.end())
    return nullptr;

  return *result;
}

void SDKDB::insertObjCProtocol(ObjCProtocolRecord *record,
                               const BinaryInfo *binInfo, StringRef project) {
  auto key = record->name;
  auto result = protocolMap.try_emplace(key, record, binInfo, project);
  if (result.second)
    return;

  auto entry = result.first;
  // If there exists protocol definition already, check they are the same.
  // no additional methods.
  auto *base = entry->getValue().getRecord();
  for (auto *method : record->methods) {
    if (!findMethod(method->name, method->isInstanceMethod, *base,
                    SDKDB::ObjCProtocol))
      builder->report(diag::warn_sdkdb_conflict_objc_protocol) << key;
  }
  // no additional protocol conformance.
  for (auto protocol : record->protocols) {
    if (find_if(base->protocols.begin(), base->protocols.end(),
                [&](auto baseProtocol) {
                  return protocol.name == baseProtocol.name;
                }) == std::end(base->protocols))
      builder->report(diag::warn_sdkdb_conflict_objc_protocol) << key;
  }

  // If entires are "equal", set the entry to poison since we don't know which
  // to pick so we pick neither.
  MapEntry<ObjCProtocolRecord*> current{record, binInfo, project};
  if (current == entry->getValue()) {
    entry->getValue().setPoison();
    return;
  }

  // Set the value if current value should replace the one in the map.
  if (current < entry->getValue())
    entry->setValue(current);
}

void SDKDB::annotateGlobal(const GlobalRecord *record) {
  auto entry = findGlobals(record->name);
  if (entry.empty()) {
    if (!record->availability._unavailable &&
        record->linkage >= APILinkage::Reexported)
      builder->report(diag::warn_sdkdb_missing_global) << record->name;
    return;
  }

  for (auto *base: entry)
    builder->updateGlobal(*base, *record);
}

void SDKDB::annotateObjCInterface(const ObjCInterfaceRecord *record) {
  auto *base = findObjCInterface(record->name);
  if (!base) {
    if (!record->availability._unavailable)
      builder->report(diag::warn_sdkdb_missing_objc_class) << record->name;
    return;
  }

  builder->updateObjCInterface(*this, *base, *record);

  // Update symbols for objc interfaces.
  if (!record->isExported())
    return;
  auto name = record->name;
  if (isObjC1())
    findAndUpdateGlobal(".objc_class_name_" + name, *record);
  else {
    findAndUpdateGlobal("_OBJC_CLASS_$_" + name, *record);
    findAndUpdateGlobal("_OBJC_METACLASS_$_" + name, *record);
    if (record->hasExceptionAttribute)
      findAndUpdateGlobal("_OBJC_EHTYPE_$_" + name, *record);
  }

  // sync ivars. ivars are not recorded in objc containers. sync ivars with
  // the symbols.
  for (auto *ivar : record->ivars) {
    if (!isObjC1() &&
        (ivar->accessControl ==
             ObjCInstanceVariableRecord::AccessControl::Public ||
         ivar->accessControl ==
             ObjCInstanceVariableRecord::AccessControl::Protected))
      findAndUpdateGlobal("_OBJC_IVAR_$_" + name + "." + ivar->name, *record);
  }
}

void SDKDB::annotateObjCProtocol(const ObjCProtocolRecord *record) {
  auto *base = findObjCProtocol(record->name);
  if (!base) {
    if (!record->availability._unavailable)
      builder->report(diag::warn_sdkdb_missing_objc_protocol) << record->name;
    return;
  }

  builder->updateObjCProtocol(*this, *base, *record);
}

void SDKDB::annotateObjCCategory(const ObjCCategoryRecord *record) {
  if (auto *base = findObjCCategory(record->name, record->interface.name))
    // For categories, search for category in the binary first.
    builder->updateObjCCategory(*this, *base, *record);
  else if (auto *cls = findObjCInterface(record->interface.name))
    // If the category is not found from mapping, annotate the base class
    // because linker might merge the category into the base class already.
    builder->updateObjCContainer(*this, *cls, *record, SDKDB::ObjCClass);
  else {
    if (!record->availability._unavailable)
      builder->report(diag::warn_sdkdb_missing_objc_category)
          << record->interface.name << record->name;
    return;
  }

  // sync ivars. ivars are not recorded in objc containers. sync ivars with
  // the symbols.
  for (auto *ivar : record->ivars) {
    if (!isObjC1() &&
        (ivar->accessControl ==
             ObjCInstanceVariableRecord::AccessControl::Public ||
         ivar->accessControl ==
             ObjCInstanceVariableRecord::AccessControl::Protected))
      findAndUpdateGlobal(
          "_OBJC_IVAR_$_" + record->interface.name + "." + ivar->name, *record);
  }
}

std::vector<const API *> SDKDB::api() const {
  std::vector<const API *> sortedAPIs;
  sortedAPIs.reserve(apiCache.size() + 1);
  for (const auto &apis : apiCache) {
    for (const auto &api : apis.second)
      sortedAPIs.emplace_back(&api);
  }
  if (!frontendAPI.isEmpty())
    sortedAPIs.emplace_back(&frontendAPI);

  llvm::stable_sort(sortedAPIs, [](const API *api1, const API *api2) {
    return *api1 < *api2;
  });
  return sortedAPIs;
}

std::vector<API *> SDKDB::api() {
  std::vector<API *> sortedAPIs;
  sortedAPIs.reserve(apiCache.size() + 1);
  for (auto &apis : apiCache) {
    for (auto &api : apis.second)
      sortedAPIs.emplace_back(&api);
  }
  if (!frontendAPI.isEmpty())
    sortedAPIs.emplace_back(&frontendAPI);

  llvm::stable_sort(sortedAPIs, [](const API *api1, const API *api2) {
    return *api1 < *api2;
  });
  return sortedAPIs;
}

std::vector<const EnumRecord*> SDKDB::getEnumRecords() const {
  std::vector<const EnumRecord*> sortedRecords;
  sortedRecords.reserve(enumMap.size());
  for (const auto &record : enumMap)
    sortedRecords.emplace_back(record.getValue().getRecord());

  llvm::sort(sortedRecords, [](const EnumRecord *r1, const EnumRecord *r2) {
    return r1->name < r2->name;
  });

  return sortedRecords;
}

std::vector<const APIRecord*> SDKDB::getTypedefRecords() const {
  std::vector<const APIRecord*> sortedRecords;
  sortedRecords.reserve(typedefMap.size());
  for (const auto &record : typedefMap)
    sortedRecords.emplace_back(record.getValue().getRecord());

  llvm::sort(sortedRecords, [](const APIRecord *r1, const APIRecord *r2) {
    return r1->name < r2->name;
  });

  return sortedRecords;
}

class APIFinalizer : public APIMutator {
public:
  APIFinalizer(SDKDBBuilder &builder, SDKDB &sdkdb)
      : builder(builder), sdkdb(sdkdb) {}

  void visitObjCProtocol(ObjCProtocolRecord &record) override {
    auto *protocol = sdkdb.findObjCProtocol(record.name);
    if (!protocol)
      return;

    // Patch up protocol list from all the binaries.
    builder.updateAPIRecord(record, *protocol);
    for (auto *method : record.methods) {
      if (auto *result = findMethodFromContainer(
              method->name, method->isInstanceMethod, *protocol))
        builder.updateAPIRecord(*method, *result);
    }
    for (auto *prop : record.properties) {
      if (auto *property = findProperty(prop->name, *protocol))
        builder.updateAPIRecord(*prop, *property);
    }
  }

private:
  SDKDBBuilder &builder;
  SDKDB &sdkdb;
};

template<typename MapEntryIter>
bool isPublicDylibEntry(const MapEntryIter &entry) {
  if (auto *binInfo = entry.getValue().getBinaryInfo()) {
    if (binInfo->fileType == FileType::MachO_Bundle)
      return false;
    return isWithinPublicLocation(binInfo->installName);
  }

  return false;
}

Error SDKDB::finalize() {
  // Finalize SDKDB.
  // Update the access of methods to public if there exists super class/protocol
  // which declares the method to be public.
  // 1. Update Protocol methods.
  for (auto &entry : protocolMap) {
    auto *protocol = entry.getValue().getRecord();
    if (entry.getValue().isPoison()) {
      builder->report(diag::warn_sdkdb_poison_entry)
          << ObjCContainerKind::ObjCProtocol << protocol->name;
      continue;
    }
    if (!isPublicDylibEntry(entry))
      continue;
    for (auto *method : protocol->methods) {
      if (method->access != APIAccess::Public &&
          builder->isMaybePublicSelector(method->name))
        method->access = getAccessForObjCMethod(
            method->access, method->name, method->isInstanceMethod, protocol);
    }
  }

  // 2. Update Interface method.
  for (auto &entry : interfaceMap) {
    auto *interface = entry.getValue().getRecord();
    if (entry.getValue().isPoison()) {
      builder->report(diag::warn_sdkdb_poison_entry)
          << ObjCContainerKind::ObjCClass << interface->name;
      continue;
    }

    if (!isPublicDylibEntry(entry))
      continue;

    for (auto *method : interface->methods) {
      if (method->access != APIAccess::Public &&
          builder->isMaybePublicSelector(method->name))
        method->access = getAccessForObjCMethod(
            method->access, method->name, method->isInstanceMethod, interface);
    }
  }

  // 3. Update Category method.
  for (auto &catEntry : categoryMap) {
    auto categories = catEntry.getValue();
    ObjCInterfaceRecord *interface = findObjCInterface(catEntry.getKey());
    for (auto &entry : categories) {
      auto *category = entry.getValue().getRecord();
      if (entry.getValue().isPoison()) {
        std::string diagName =
            category->interface.name.str() + "(" + category->name.str() + ")";
        builder->report(diag::warn_sdkdb_poison_entry)
            << ObjCContainerKind::ObjCCategory << diagName;
        continue;
      }
      if (!isPublicDylibEntry(entry))
        continue;
      for (auto *method : category->methods) {
        if (method->access != APIAccess::Public &&
          builder->isMaybePublicSelector(method->name)) {
          method->access =
              getAccessForObjCMethod(method->access, method->name,
                                     method->isInstanceMethod, category);
          // Look at base class if exists.
          if (method->access != APIAccess::Public && interface)
            method->access =
                getAccessForObjCMethod(method->access, method->name,
                                       method->isInstanceMethod, interface);
        }
      }
    }
  }

  // Perform SDKDB finalize.
  // 1. Fixup all the protocols in the SDKDB.
  // 2. Apply dylib promote to promote to public list to the entry.
  for (auto *api: api()) {
    if (!api->hasBinaryInfo() ||
        api->getBinaryInfo().fileType == FileType::MachO_Bundle)
      continue;

    if (!isPublicDylib(api->getBinaryInfo().installName))
      continue;

    APIFinalizer updater(*builder, *this);
    api->visit(updater);
  }

  return Error::success();
}

ObjCInterfaceRecord *SDKDB::getSuperclass(const ObjCInterfaceRecord *record) {
  if (record->superClass.name.empty())
    return nullptr;

  return findObjCInterface(record->superClass.name);
}

APIAccess SDKDB::getAccessForObjCMethod(APIAccess access, StringRef name,
                                        bool isInstanceMethod,
                                        ObjCContainerRecord *container) {
  // check current container for the access.
  for (auto *method : container->methods) {
    if (method->name == name && method->isInstanceMethod == isInstanceMethod)
      access = method->access > access ? method->access : access;
  }
  if (access == APIAccess::Public)
    return access; // return since it is already public.

  // walk protocol hierarchy.
  for (auto protocolSymbol : container->protocols) {
    if (auto *protocol = findObjCProtocol(protocolSymbol.name))
      access = getAccessForObjCMethod(access, name, isInstanceMethod, protocol);
    if (access == APIAccess::Public)
      return access;
  }

  return access;
}

APIAccess SDKDB::getAccessForObjCMethod(APIAccess access, StringRef name,
                                        bool isInstanceMethod,
                                        ObjCInterfaceRecord *interface) {
  // walk the common container part first.
  access = getAccessForObjCMethod(access, name, isInstanceMethod,
                                  (ObjCContainerRecord *)interface);
  if (access == APIAccess::Public)
    return access; // return since it is already public.

  // check super class.
  if (auto *super = getSuperclass(interface))
    access = getAccessForObjCMethod(access, name, isInstanceMethod, super);
  if (access == APIAccess::Public)
    return access; // return since it is already public.

  // check category. category should not overwrite the methods from interface
  // but it can introduce new procotol conformance.
  for (auto &category : findObjCCategoryForClass(interface->name)) {
    for (auto protocolSymbol : category->protocols) {
      if (auto *protocol = findObjCProtocol(protocolSymbol.name))
        access =
            getAccessForObjCMethod(access, name, isInstanceMethod, protocol);
      if (access == APIAccess::Public)
        return access;
    }
  }

  return access;
}

bool SDKDB::findAndUpdateGlobal(Twine name, const APIRecord &record) {
  auto syms = findGlobals(name.str());
  if (syms.empty())
    return false;
  for (auto *value : syms)
    builder->updateAPIRecord(*value, record);
  return true;
}

ObjCMethodRecord *SDKDB::findMethod(StringRef name, bool isInstanceMethod,
                                    ObjCContainerRecord &record,
                                    ObjCContainerKind kind,
                                    StringRef fallbackInterfaceName) {
  auto *result = findMethodFromContainer(name, isInstanceMethod, record);
  if (result)
    return result;

  if (kind == SDKDB::ObjCCategory) {
    // Find the base class and recursively search base class.
    if (auto *cls = findObjCInterface(fallbackInterfaceName))
      return findMethod(name, isInstanceMethod, *cls, SDKDB::ObjCClass);
  } else if (kind == SDKDB::ObjCClass) {
    // Only do global search for objc classes.
    for (auto &category : findObjCCategoryForClass(record.name)) {
      result = findMethodFromContainer(name, isInstanceMethod, *category);
      if (result)
        return result;
    }
  }

  return nullptr;
}

Error SDKDBBuilder::addBinaryAPI(API &&api) {
  auto &db = getSDKDBForTarget(api.getTarget());
  auto &current = db.recordAPI(std::move(api));

  // No need to put bundle into lookup map.
  if (api.getBinaryInfo().fileType == FileType::MachO_Bundle)
    return Error::success();

  // Build lookup map.
  LookupMapBuilder builder(db, current);
  current.visit(builder);

  return Error::success();
}

Error SDKDBBuilder::addHeaderAPI(const API &api) {
  auto &db = getSDKDBForTarget(api.getTarget());
  APIAnnotator annotator(db, api.getProjectName());
  api.visit(annotator);

  return Error::success();
}

void SDKDBBuilder::updateAPIRecord(APIRecord &base, const APIRecord &record) {
  // update the APLoc if the original location is invalid.
  // also moving away from clang::PresumedLoc because it is not in the same
  // context.
  if (base.loc.isInvalid() && preserveLocation())
    base.loc = APILoc(record.loc.getFilename(), record.loc.getLine(),
                      record.loc.getColumn());

  if (base.availability.isDefault())
    base.availability = record.availability;
  else if (base.availability != record.availability &&
           !record.availability.isDefault()) {
    diag.report(diag::warn_sdkdb_conflict_availability)
        << base.name << base.availability.str() << record.availability.str();
    // Pick the newer availability to be consistent.
    if (base.availability < record.availability)
      base.availability = record.availability;
  }

  if (base.access < record.access)
    base.access = record.access;

  // TODO: update USR after finding a way to properly copy the string data.
}

void SDKDBBuilder::updateObjCContainer(SDKDB &sdkdb, ObjCContainerRecord &base,
                                       const ObjCContainerRecord &record,
                                       SDKDB::ObjCContainerKind kind,
                                       StringRef fallbackInterfaceName) {
  updateAPIRecord(base, record);

  auto handleMissingMethod = [&](StringRef selectorName,
                                 APIRecord &selectorInfo,
                                 FunctionSignature signature,
                                 bool isInstanceMethod, bool isOptional,
                                 bool isDynamic) {
    diag.report(diag::warn_sdkdb_missing_objc_method)
        << selectorName << kind << record.name;

    APILoc location;
    if (preserveLocation())
      location = selectorInfo.loc;
    auto *m = ObjCMethodRecord::create(
        sdkdb.danglingAPIAllocator, selectorName, selectorInfo.usr, location,
        selectorInfo.availability, selectorInfo.access, isInstanceMethod,
        isOptional, isDynamic, selectorInfo.docComment,
        selectorInfo.declarationFragments, selectorInfo.subHeading, signature,
        selectorInfo.decl);
    base.methods.push_back(m);
  };

  for (auto *method : record.methods) {
    if (auto *baseMethod =
            sdkdb.findMethod(method->name, method->isInstanceMethod, base, kind,
                             fallbackInterfaceName))
      updateAPIRecord(*baseMethod, *method);
    else if (!method->availability._unavailable)
      handleMissingMethod(method->name, *method, method->signature,
                          method->isInstanceMethod, method->isOptional,
                          method->isDynamic);

    if (method->access == APIAccess::Public)
      maybePublicSelector.insert(method->name);
  }

  // base does not have properties because it is coming from binary interface.
  // use the property in header to annotate getter and setter inside base.
  for (auto *prop : record.properties) {
    bool available = !prop->availability._unavailable;
    // dynamic is only known to implementation. Default to false.
    bool isDynamic = false;
    // use the class property attributes from header.
    bool isClassProperty = prop->isClassProperty();
    if (auto *property = findProperty(prop->name, base)) {
      updateAPIRecord(*property, *prop);
      isDynamic = property->isDynamic();
    } else if (available)
      diag.report(diag::warn_sdkdb_missing_objc_property)
          << prop->name << kind << base.name;

    // dynamic property doesn't have synthesized methods in class.
    if (isDynamic)
      continue;

    if (auto *baseMethod = sdkdb.findMethod(prop->getterName,
                                            /*instanceMethod*/ !isClassProperty,
                                            base, kind, fallbackInterfaceName))
      updateAPIRecord(*baseMethod, *prop);
    else if (!prop->isOptional && available)
      handleMissingMethod(prop->getterName, *prop, /* signature */ {},
                          /* isInstanceMethod */ !isClassProperty,
                          /* isOptional */ false, /* isDynamic */ false);

    if (prop->access == APIAccess::Public)
      maybePublicSelector.insert(prop->getterName);

    if (prop->isReadOnly())
      continue;

    if (auto *baseMethod = sdkdb.findMethod(prop->setterName,
                                            /*instanceMethod*/ !isClassProperty,
                                            base, kind, fallbackInterfaceName))
      updateAPIRecord(*baseMethod, *prop);
    else if (!prop->isOptional && available)
      handleMissingMethod(prop->setterName, *prop, /* signature */ {},
                          /* isInstanceMethod */ !isClassProperty,
                          /* isOptional */ false, /* isDynamic */ false);

    if (prop->access == APIAccess::Public)
      maybePublicSelector.insert(prop->setterName);
  }

  // TODO: Now we just sync all the conformed protocol from header to API.
  // Need to teach MachOReader to read them from binary in the future.
  for (auto protocol : record.protocols) {
    if (find_if(base.protocols.begin(), base.protocols.end(),
                [&](auto baseProtocol) {
                  return protocol.name == baseProtocol.name;
                }) == std::end(base.protocols))
      base.protocols.push_back(protocol);
  };
}

void SDKDBBuilder::updateGlobal(GlobalRecord &base,
                                const GlobalRecord &record) {
  updateAPIRecord(base, record);

  if (base.kind == GVKind::Unknown)
    base.kind = record.kind;
  else if (base.kind != record.kind)
    diag.report(diag::warn_sdkdb_conflict_gvkind) << base.name;
}

void SDKDBBuilder::updateObjCInterface(SDKDB &sdkdb, ObjCInterfaceRecord &base,
                                       const ObjCInterfaceRecord &record) {
  updateObjCContainer(sdkdb, base, record, SDKDB::ObjCClass);
  if (base.superClass.name != record.superClass.name)
    diag.report(diag::warn_sdkdb_conflict_superclass)
        << base.name << base.superClass.name << record.superClass.name;
  base.hasExceptionAttribute |= record.hasExceptionAttribute;
}

void SDKDBBuilder::updateObjCCategory(SDKDB &sdkdb, ObjCCategoryRecord &base,
                                      const ObjCCategoryRecord &record) {
  updateObjCContainer(sdkdb, base, record, SDKDB::ObjCCategory,
                      record.interface.name);
}

void SDKDBBuilder::updateObjCProtocol(SDKDB &sdkdb, ObjCProtocolRecord &base,
                                      const ObjCProtocolRecord &record) {
  updateObjCContainer(sdkdb, base, record, SDKDB::ObjCProtocol);
}

SDKDB &SDKDBBuilder::getSDKDBForTarget(const Triple &triple) {
  for (auto &entry : databases) {
    if (SDKDB::areCompatibleTargets(entry.getTargetTriple(), triple))
      return entry;
  }

  Triple key(triple);
  // clear the os version so it is not subjected to the first version it tries
  // to insert into SDKDB.
  key.setOSName(Triple::getOSTypeName(triple.getOS()));
  databases.emplace_back(key, this);
  return databases.back();
}

Error SDKDBBuilder::finalize() {
  for (auto &entry : databases) {
    if (auto err = entry.finalize())
      return err;
  }

  // sort projectWithError
  llvm::sort(projectWithError);
  return Error::success();
}

llvm::Error SDKDBBuilder::parse(StringRef JSON) {
  auto inputValue = json::parse(JSON);
  if (!inputValue)
    return inputValue.takeError();

  auto *root = inputValue->getAsObject();
  if (!root)
    return make_error<APIJSONError>("SDKDB is not a JSON Object");

  for (auto &target : *root) {
    auto triple = Triple(target.first.str());
    auto payload = target.second.getAsArray();
    if (!payload)
      return make_error<APIJSONError>("Target Payload is not a JSON Array");

    for (auto &value : *payload) {
      auto obj = value.getAsObject();
      if (!obj)
        return make_error<APIJSONError>(
            "SDKDB doesn't include correct API format");
      auto api = APIJSONSerializer::parse(obj, isPublicOnly(), &triple);
      if (!api)
        return api.takeError();

      if (auto err = addBinaryAPI(std::move(*api)))
        return err;
    }
  }

  return Error::success();
}

std::vector<const SDKDB*> SDKDBBuilder::getDatabases() const {
  std::vector<Triple> sortedTriples;
  for (auto &entry : databases)
    sortedTriples.emplace_back(entry.getTargetTriple());
  llvm::sort(sortedTriples, [](const Triple &t1, const Triple &t2) {
    return t1.str() < t2.str();
  });

  std::vector<const SDKDB*> sortedDatabases;
  for (auto &target : sortedTriples) {
    const auto *entry = llvm::find_if(databases, [&](const SDKDB &db) {
      return db.getTargetTriple().str() == target.str();
    });
    assert(entry != databases.end() && "Target must exists.");
    sortedDatabases.emplace_back(entry);
  }
  return sortedDatabases;
}

void SDKDBBuilder::serialize(raw_ostream &os, bool compact) const {
  json::Object root;
  APIJSONOption serializeOpts = {
      compact,
      !hasUUID(),
      /*no target*/ true,
      /*external only*/ true,
      isPublicOnly(),
      /*ignore line and col*/ true,
      /*no USR*/ true,
      /*no docComment*/ true,
      /*noDeprecationInfo*/ true,
      /*noStruct*/ true,
      /*noDeclName*/ true,
      /*no declFragments*/ true,
      /*noElaboratedSymbolInfo*/ true,
      /*noMacroDefinitions*/ true,
      /*noUnifiedTypedefEntries*/ true,
  };
  for (auto *entry : getDatabases()) {
    auto target = entry->getTargetTriple();
    json::Array apiList;
    for (auto *api : entry->api()) {
      APIJSONSerializer serializer(*api, serializeOpts);
      apiList.emplace_back(serializer.getJSONObject());
    }
    root[target.str()] = std::move(apiList);
  }
  if (isPublicOnly())
    root["public"] = true;

  json::Array projects;
  for (auto &proj : projectWithError)
    projects.push_back(proj);
  if (!projects.empty())
    root["projectWithError"] = std::move(projects);

  if (compact)
    os << formatv("{0}", json::Value(std::move(root))) << "\n";
  else
    os << formatv("{0:2}", json::Value(std::move(root))) << "\n";
}

template <typename LookupMapTy>
inline std::set<StringRef> allKeysFromMaps(const LookupMapTy &base,
                                           const LookupMapTy &test) {
  std::set<StringRef> keys;
  keys.insert(base.keys().begin(), base.keys().end());
  keys.insert(test.keys().begin(), test.keys().end());
  return keys;
}

template <typename ContainerTy>
inline std::set<StringRef> allKeysFromContainer(const ContainerTy &base,
                                                const ContainerTy &test) {
  std::set<StringRef> keys;
  for (auto &m : base)
    keys.insert(m->name);
  for (auto &m : test)
    keys.insert(m->name);
  return keys;
}

template <typename LookupMapTy>
inline std::set<std::pair<StringRef, StringRef>>
allKeyPairsFromNestedMaps(const LookupMapTy &base, const LookupMapTy &test) {
  std::set<std::pair<StringRef, StringRef>> keys;
  for (auto &k1 : base) {
    for (auto &k2 : k1.getValue())
      keys.emplace(k1.getKey(), k2.getKey());
  }
  for (auto &k1 : test) {
    for (auto &k2 : k1.getValue())
      keys.emplace(k1.getKey(), k2.getKey());
  }
  return keys;
}

static bool checkAPIRecord(const APIRecord &record, const APIRecord &base,
                           std::function<void(StringRef)> handler) {
  assert(record.name == base.name && "record names are not equal");
  if (base.access != APIAccess::Public)
    return true; // allow regression for non-public API.

  if (record.access != APIAccess::Public) {
    handler("api access regression");
    return false;
  }

  if (record.linkage != base.linkage) {
    handler("linkage type is not equal");
    return false;
  }

  if (!base.availability.isDefault() &&
      record.availability != base.availability) {
    handler("availability changed");
    return false;
  }
  return true;
}

static bool checkObjCContainer(
    const ObjCContainerRecord &test, const ObjCContainerRecord &base,
    std::function<void(StringRef)> handlerForContainer,
    std::function<void(StringRef)> handlerForNewSelector,
    std::function<void(StringRef, StringRef)> handlerForRegressSelector) {
  bool regression = checkAPIRecord(test, base, handlerForContainer);

  for (auto m : allKeysFromContainer(base.methods, test.methods)) {
    auto bm = llvm::find_if(base.methods, [&](const ObjCMethodRecord *record) {
      return record->name == m;
    });
    auto tm = llvm::find_if(test.methods, [&](const ObjCMethodRecord *record) {
      return record->name == m;
    });
    // regression.
    if (tm == test.methods.end()) {
      assert(bm != base.methods.end() && "baseline should exist");
      // ignore the private APIs.
      auto *missing = *bm;
      if (missing->access != APIAccess::Public)
        continue;

      handlerForRegressSelector(m, "selector is missing");
      continue;
    }

    // new API.
    if (bm == base.methods.end()) {
      assert(tm != test.methods.end() && "test version should exist");
      auto *missing = *tm;
      if (missing->access != APIAccess::Public)
        continue;

      handlerForNewSelector(m);
      continue;
    }

    // new API case 2.
    if ((*bm)->access != APIAccess::Public &&
        (*tm)->access == APIAccess::Public) {
      handlerForNewSelector(m);
      continue;
    }

    checkAPIRecord(**tm, **bm, [&](StringRef error) {
      handlerForRegressSelector(m, error);
    });
  }

  return regression;
}

void SDKDB::buildLookupTables() {
  assert(globalMap.empty() && interfaceMap.empty() && categoryMap.empty() &&
         protocolMap.empty() && "Lookup table should not be built yet");
  for (auto *api : api()) {
    LookupMapBuilder builder(*this, *api);
    api->visit(builder);
  }

  // sort the global entries.
  for (auto &entry : globalMap)
    llvm::sort(entry.second);
}

template <typename MapEntryIter>
bool shouldDiagnoseEntry(const MapEntryIter &entry) {
  // Skip all the references in PrivateFrameworks.
  // FIXME: This is a workaround to make diffs less noisy. It would be good to
  // reduce those cases by only proprogate or compare the private framework
  // that are re-exported.
  if (entry.getRecord()->access != APIAccess::Public)
    return false;

  if (auto *binInfo = entry.getBinaryInfo()) {
    if (binInfo->fileType == FileType::MachO_Bundle)
      return false;
    return isWithinPublicLocation(binInfo->installName);
  }

  return true;
}

void SDKDB::diagnoseDifferences(const SDKDB &baseline) const {
  // Diff APIs by diffing the global lookup table to see if there are
  // changes to the APIs.
  // 1. check globals.
  for (auto name : allKeysFromMaps(baseline.globalMap, globalMap)) {
    auto base = baseline.globalMap.find(name);
    auto test = globalMap.find(name);

    // regression.
    if (test == globalMap.end()) {
      assert(base != baseline.globalMap.end() && "baseline should exist");
      for (auto missing : base->second) {
        // ignore the private APIs.
        if (!shouldDiagnoseEntry(missing))
          continue;

        builder->report(diag::err_sdkdb_missing_global)
            << (unsigned)missing.getRecord()->kind << name
            << missing.getBinaryInfo()->installName << getTargetTriple().str();
      }
      continue;
    }

    // new API.
    if (base == baseline.globalMap.end()) {
      assert(test != globalMap.end() && "test version should exist");
      for (auto missing : test->second) {
        if (!shouldDiagnoseEntry(missing))
          continue;
        builder->report(diag::warn_sdkdb_new_global)
            << (unsigned)missing.getRecord()->kind << name
            << missing.getBinaryInfo()->installName << getTargetTriple().str();
      }
      continue;
    }

    // Compare entries. All entries are sorted.
    unsigned baseIdx = 0, testIdx = 0;
    while (baseIdx < base->second.size()) {
      auto baseEntry = base->second[baseIdx];
      // Advance testIdx if testEntry is smaller.
      while (testIdx < test->second.size() &&
             test->second[testIdx] < baseEntry) {
        // new APIs case 2.
        if (shouldDiagnoseEntry(test->second[testIdx])) {
          builder->report(diag::warn_sdkdb_new_global)
              << (unsigned)baseEntry.getRecord()->kind << name
              << baseEntry.getBinaryInfo()->installName
              << getTargetTriple().str();
        }
        ++testIdx;
      }

      // If there isn't a matching entry for base, report regressions.
      if (testIdx >= test->second.size() ||
          baseEntry != test->second[testIdx]) {
        if (shouldDiagnoseEntry(baseEntry)) {
          builder->report(diag::err_sdkdb_missing_global)
              << (unsigned)baseEntry.getRecord()->kind << name
              << baseEntry.getBinaryInfo()->installName
              << getTargetTriple().str();
        }
        ++baseIdx;
        continue;
      }

      // Diff two matching entries.
      auto &testEntry = test->second[testIdx];
      // First, check if this is a newly added API.
      if (baseEntry.getRecord()->access != APIAccess::Public &&
          shouldDiagnoseEntry(testEntry)) {
        builder->report(diag::warn_sdkdb_new_global)
            << (unsigned)testEntry.getRecord()->kind << name
            << testEntry.getBinaryInfo()->installName
            << getTargetTriple().str();
        ++baseIdx;
        ++testIdx;
        continue;
      }

      // Second, check annotation on the matching entries.
      auto *record = baseEntry.getRecord();
      auto installName = baseEntry.getBinaryInfo()->installName;
      checkAPIRecord(*testEntry.getRecord(), *record, [&](StringRef error) {
        builder->report(diag::err_sdkdb_global_regression)
            << name << installName << getTargetTriple().str() << error;
      });
      ++baseIdx;
      ++testIdx;
    }

    // Check remaining test entries for new APIs.
    while (testIdx < test->second.size()) {
      auto &testEntry = test->second[testIdx];
      if (shouldDiagnoseEntry(testEntry)) {
        builder->report(diag::warn_sdkdb_new_global)
            << (unsigned)testEntry.getRecord()->kind << name
            << testEntry.getBinaryInfo()->installName
            << getTargetTriple().str();
      }
      ++testIdx;
    }
  }

  // 2. check objc classes.
  for (auto name : allKeysFromMaps(baseline.interfaceMap, interfaceMap)) {
    auto base = baseline.interfaceMap.find(name);
    auto test = interfaceMap.find(name);
    // regression.
    if (test == interfaceMap.end()) {
      assert(base != baseline.interfaceMap.end() && "baseline should exist");
      // ignore the private APIs.
      auto missing = base->second;
      if (!shouldDiagnoseEntry(missing))
        continue;

      builder->report(diag::err_sdkdb_missing_objc)
          << 0 << name << missing.getBinaryInfo()->installName
          << getTargetTriple().str();
      continue;
    }

    // new API.
    if (base == baseline.interfaceMap.end()) {
      assert(test != interfaceMap.end() && "test version should exist");
      auto missing = test->second;
      if (!shouldDiagnoseEntry(missing))
        continue;
      builder->report(diag::warn_sdkdb_new_objc)
          << 0 << name << missing.getBinaryInfo()->installName
          << getTargetTriple().str();
      continue;
    }

    // new API case 2. Promoted from existing class.
    if (base->second.getRecord()->access != APIAccess::Public &&
        shouldDiagnoseEntry(test->second)) {
      builder->report(diag::warn_sdkdb_new_objc)
          << 0 << name << test->second.getBinaryInfo()->installName
          << getTargetTriple().str();
      continue;
    }

    if (!shouldDiagnoseEntry(base->second))
      continue;

    auto installName = base->second.getBinaryInfo()->installName;
    checkObjCContainer(
        *test->second.getRecord(), *base->second.getRecord(),
        [&](StringRef error) {
          builder->report(diag::err_sdkdb_objc_container_regression)
              << 0 << name << installName << getTargetTriple().str() << error;
        },
        [&](StringRef selector) {
          builder->report(diag::warn_sdkdb_new_objc_selector)
              << selector << 0 << name << installName
              << getTargetTriple().str();
        },
        [&](StringRef selector, StringRef error) {
          builder->report(diag::err_sdkdb_objc_selector_regression)
              << selector << 0 << name << installName << getTargetTriple().str()
              << error;
        });
  }

  // 3. check objc categories.
  for (auto names :
       allKeyPairsFromNestedMaps(baseline.categoryMap, categoryMap)) {
    auto findCategory = [&](const SDKDB::CategoryMapType &map)
        -> Optional<MapEntry<ObjCCategoryRecord *>> {
      auto clsRes = map.find(names.first);
      if (clsRes == map.end())
        return llvm::None;

      auto res = clsRes->second.find(names.second);
      if (res == clsRes->second.end())
        return llvm::None;

      return res->second;
    };

    auto base = findCategory(baseline.categoryMap);
    auto test = findCategory(categoryMap);
    std::string name = names.second.str() + "(" + names.first.str() + ")";

    // regression.
    if (!test) {
      assert(base && "baseline should exist");
      // ignore the private APIs.
      auto missing = *base;
      if (!shouldDiagnoseEntry(missing))
        continue;

      builder->report(diag::err_sdkdb_missing_objc)
          << 1 << name << missing.getBinaryInfo()->installName
          << getTargetTriple().str();
      continue;
    }

    // new API.
    if (!base) {
      assert(test && "test version should exist");
      auto missing = *test;
      if (!shouldDiagnoseEntry(missing))
        continue;
      builder->report(diag::warn_sdkdb_new_objc)
          << 1 << name << missing.getBinaryInfo()->installName
          << getTargetTriple().str();
      continue;
    }

    // new API case 2. Promoted from existing categories.
    if (base->getRecord()->access != APIAccess::Public &&
        shouldDiagnoseEntry(*test)) {
      builder->report(diag::warn_sdkdb_new_objc)
          << 1 << name << test->getBinaryInfo()->installName
          << getTargetTriple().str();
      continue;
    }

    if (!shouldDiagnoseEntry(*base))
      continue;

    auto installName = base->getBinaryInfo()->installName;
    checkObjCContainer(
        *test->getRecord(), *base->getRecord(),
        [&](StringRef error) {
          builder->report(diag::err_sdkdb_objc_container_regression)
              << 1 << name << installName << getTargetTriple().str() << error;
        },
        [&](StringRef selector) {
          builder->report(diag::warn_sdkdb_new_objc_selector)
              << selector << 1 << name << installName
              << getTargetTriple().str();
        },
        [&](StringRef selector, StringRef error) {
          builder->report(diag::err_sdkdb_objc_selector_regression)
              << selector << 1 << name << installName << getTargetTriple().str()
              << error;
        });

  }

  // 4. check objc protocols.
  for (auto name : allKeysFromMaps(baseline.protocolMap, protocolMap)) {
    auto base = baseline.protocolMap.find(name);
    auto test = protocolMap.find(name);
    // regression.
    if (test == protocolMap.end()) {
      assert(base != baseline.protocolMap.end() && "baseline should exist");
      // ignore the private APIs.
      auto missing = base->second;
      if (!shouldDiagnoseEntry(missing))
        continue;

      builder->report(diag::err_sdkdb_missing_objc)
          << 2 << name << missing.getBinaryInfo()->installName
          << getTargetTriple().str();
      continue;
    }

    // new API.
    if (base == baseline.protocolMap.end()) {
      assert(test != protocolMap.end() && "test version should exist");
      auto missing = test->second;
      if (!shouldDiagnoseEntry(missing))
        continue;
      builder->report(diag::warn_sdkdb_new_objc)
          << 2 << name << missing.getBinaryInfo()->installName
          << getTargetTriple().str();
      continue;
    }

    // new API case 2. Promoted from existing protocol.
    if (base->second.getRecord()->access != APIAccess::Public &&
        shouldDiagnoseEntry(test->second)) {
      builder->report(diag::warn_sdkdb_new_objc)
          << 2 << name << test->second.getBinaryInfo()->installName
          << getTargetTriple().str();
      continue;
    }

    if (!shouldDiagnoseEntry(base->second))
      continue;

    auto installName = base->second.getBinaryInfo()->installName;
    checkObjCContainer(
        *test->second.getRecord(), *base->second.getRecord(),
        [&](StringRef error) {
          builder->report(diag::err_sdkdb_objc_container_regression)
              << 2 << name << installName << getTargetTriple().str() << error;
        },
        [&](StringRef selector) {
          builder->report(diag::warn_sdkdb_new_objc_selector)
              << selector << 2 << name << installName
              << getTargetTriple().str();
        },
        [&](StringRef selector, StringRef error) {
          builder->report(diag::err_sdkdb_objc_selector_regression)
              << selector << 2 << name << installName << getTargetTriple().str()
              << error;
        });
  }

  // 5. check enums.
  for (auto name : allKeysFromMaps(baseline.enumMap, enumMap)) {
    auto base = baseline.enumMap.find(name);
    auto test = enumMap.find(name);
    // regression.
    if (test == enumMap.end()) {
      assert(base != baseline.enumMap.end() && "baseline should exist");
      // ignore the private APIs.
      auto missing = base->second;
      if (missing.getRecord()->access != APIAccess::Public)
        continue;

      builder->report(diag::err_sdkdb_missing_frontend_api)
          << 0 << name << getTargetTriple().str();
      continue;
    }

    // new API.
    if (base == baseline.enumMap.end()) {
      assert(test != enumMap.end() && "test version should exist");
      auto missing = test->second;
      if (missing.getRecord()->access != APIAccess::Public)
        continue;
      builder->report(diag::warn_sdkdb_new_frontend_api)
          << 0 << name << getTargetTriple().str();
      continue;
    }

    // new API case 2. Promoted from existing enums.
    if (base->second.getRecord()->access != APIAccess::Public &&
        test->second.getRecord()->access == APIAccess::Public) {
      builder->report(diag::warn_sdkdb_new_frontend_api)
          << 0 << name << getTargetTriple().str();
      continue;
    }

    // check all the fields.
    checkAPIRecord(*test->second.getRecord(), *base->second.getRecord(),
                   [&](StringRef error) {
                     builder->report(diag::err_sdkdb_frontend_api_regression)
                         << 0 << name << getTargetTriple().str() << error;
                   });

    // check constants.
    auto &baseConsts = base->second.getRecord()->constants;
    auto &testConsts = test->second.getRecord()->constants;
    for (auto c : allKeysFromContainer(baseConsts, testConsts)) {
      auto bc =
          llvm::find_if(baseConsts, [&](const EnumConstantRecord *record) {
            return record->name == c;
          });
      auto tc =
          llvm::find_if(testConsts, [&](const EnumConstantRecord *record) {
            return record->name == c;
          });
      // regression.
      if (tc == testConsts.end()) {
        assert(bc != baseConsts.end() && "baseline should exist");
        // ignore the private APIs.
        auto *missing = *bc;
        if (missing->access != APIAccess::Public)
          continue;

        builder->report(diag::err_sdkdb_missing_frontend_api)
            << 1 << c << getTargetTriple().str();
        continue;
      }

      // new API.
      if (bc == baseConsts.end()) {
        assert(tc != testConsts.end() && "test version should exist");
        auto *missing = *tc;
        if (missing->access != APIAccess::Public)
          continue;
        builder->report(diag::warn_sdkdb_new_frontend_api)
            << 1 << c << getTargetTriple().str();
        continue;
      }

      // new API case 2.
      if ((*bc)->access != APIAccess::Public &&
          (*tc)->access == APIAccess::Public) {
        builder->report(diag::warn_sdkdb_new_frontend_api)
            << 1 << c << getTargetTriple().str();
        continue;
      }

      checkAPIRecord(**tc, **bc, [&](StringRef error) {
        builder->report(diag::err_sdkdb_frontend_api_regression)
            << 2 << c << getTargetTriple().str() << error;
      });
    }
  }

  // 6. check typedef.
  for (auto name : allKeysFromMaps(baseline.typedefMap, typedefMap)) {
    auto base = baseline.typedefMap.find(name);
    auto test = typedefMap.find(name);
    // regression.
    if (test == typedefMap.end()) {
      assert(base != baseline.typedefMap.end() && "baseline should exist");
      // ignore the private APIs.
      auto missing = base->second;
      if (missing.getRecord()->access != APIAccess::Public)
        continue;

      builder->report(diag::err_sdkdb_missing_frontend_api)
          << 2 << name << getTargetTriple().str();
      continue;
    }

    // new API.
    if (base == baseline.typedefMap.end()) {
      assert(test != typedefMap.end() && "test version should exist");
      auto missing = test->second;
      if (missing.getRecord()->access != APIAccess::Public)
        continue;
      builder->report(diag::warn_sdkdb_new_frontend_api)
          << 2 << name << getTargetTriple().str();
      continue;
    }

    // new API case 2.
    if (base->second.getRecord()->access != APIAccess::Public &&
        test->second.getRecord()->access == APIAccess::Public) {
      builder->report(diag::warn_sdkdb_new_frontend_api)
          << 2 << name << getTargetTriple().str();
      continue;
    }

    // check all the fields.
    checkAPIRecord(*test->second.getRecord(), *base->second.getRecord(),
                   [&](StringRef error) {
                     builder->report(diag::err_sdkdb_frontend_api_regression)
                         << 2 << name << getTargetTriple().str() << error;
                   });
  }
}

void SDKDBBuilder::buildLookupTables() {
  for (auto &db : databases)
    db.buildLookupTables();
}

bool SDKDBBuilder::diagnoseDifferences(SDKDBBuilder &baseline) {
  // Initialize the lookup table first.
  baseline.buildLookupTables();
  buildLookupTables();

  assert(!diag.hasErrorOccurred() && "no error should occured");
  // Compare the SDKDBs by lookup table.
  for (const auto &base : baseline.databases) {
    // Check to see if the target exists.
    auto *db = llvm::find_if(databases, [&](const SDKDB &db) {
      return SDKDB::areCompatibleTargets(db.triple, base.triple);
    });
    if (db == databases.end()) {
      report(diag::err_sdkdb_missing_target) << base.triple.str();
      continue;
    }

    db->diagnoseDifferences(base);
  }

  return !diag.hasErrorOccurred();
}

void SDKDBBuilder::setReportNewAPIasError(bool val) {
  diag.setWarningsAsErrors(val);
}


TAPI_NAMESPACE_INTERNAL_END
