//===- lib/Core/APIPrinter.cpp - TAPI API Printer ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "tapi/Core/APIPrinter.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

TAPI_NAMESPACE_INTERNAL_BEGIN

APIPrinter::APIPrinter(raw_ostream &os, bool useColor)
    : os(os), hasColors(useColor && os.has_colors()) {}

APIPrinter::~APIPrinter() {}

static void printLocation(raw_ostream &os, const APILoc &loc, bool hasColors) {
  // skip invalid location.
  if (loc.isInvalid())
    return;

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  loc: ";
  if (hasColors)
    os.resetColor();
  os << loc.getFilename() << ":" << loc.getLine() << ":" << loc.getColumn()
     << "\n";
}

static void printAvailability(raw_ostream &os, const AvailabilityInfo &avail,
                              bool hasColors) {
  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  availability: ";
  if (hasColors)
    os.resetColor();
  os << avail << "\n";
}

static void printLinkage(raw_ostream &os, APILinkage linkage, bool hasColors) {
  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  linkage: ";
  if (hasColors)
    os.resetColor();
  switch (linkage) {
  case APILinkage::Exported:
    os << "exported";
    break;
  case APILinkage::Reexported:
    os << "re-exported";
    break;
  case APILinkage::Internal:
    os << "internal";
    break;
  case APILinkage::External:
    os << "external";
    break;
  case APILinkage::Unknown:
    os << "unknown";
    break;
  }
  os << "\n";
}

static void printUSR(raw_ostream &os, StringRef usr, bool hasColors,
                     unsigned indent = 2) {
  // Skip empty USR.
  if (usr.empty())
    return;

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os.indent(indent) << "USR: ";
  if (hasColors)
    os.resetColor();
  os << usr << "\n";
}

static void printAPIRecord(raw_ostream &os, const APIRecord &var,
                           bool hasColors) {
  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "- name: ";
  if (hasColors)
    os.resetColor();
  os << var.name << "\n";

  printUSR(os, var.usr, hasColors);
  printLocation(os, var.loc, hasColors);
  printAvailability(os, var.availability, hasColors);
  // TODO: Print out docComment.

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  access: ";
  if (hasColors)
    os.resetColor();
  switch (var.access) {
  case APIAccess::Public:
    os << "public";
    break;
  case APIAccess::Private:
    os << "private";
    break;
  case APIAccess::Project:
    os << "project";
    break;
  case APIAccess::Unknown:
    os << "unknown";
    break;
  }
  os << "\n";
}

void APIPrinter::visitMacroDefinition(const MacroDefinitionRecord &macro) {
  if (!emittedHeaderMacro) {
    if (hasColors)
      os.changeColor(raw_ostream::GREEN);
    os << "macros:\n";
    if (hasColors)
      os.resetColor();
    emittedHeaderMacro = true;
  }
  printAPIRecord(os, macro, hasColors);
}

static void printGlobalRecord(raw_ostream &os, const GlobalRecord &var,
                              bool hasColors) {
  printAPIRecord(os, var, hasColors);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  isWeakDefined: ";
  if (hasColors)
    os.resetColor();
  os << (var.isWeakDefined() ? "true" : "false") << "\n";

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  isThreadLocalValue: ";
  if (hasColors)
    os.resetColor();
  os << (var.isThreadLocalValue() ? "true" : "false") << "\n";

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  kind: ";
  if (hasColors)
    os.resetColor();
  switch (var.kind) {
  case GVKind::Function:
    os << "function";
    break;
  case GVKind::Variable:
    os << "variable";
    break;
  case GVKind::Unknown:
    os << "unknown";
    break;
  }
  os << "\n";

  printLinkage(os, var.linkage, hasColors);
}

void APIPrinter::visitGlobal(const GlobalRecord &var) {
  if (!emittedHeaderGlobal) {
    if (hasColors)
      os.changeColor(raw_ostream::GREEN);
    os << "globals:\n";
    if (hasColors)
      os.resetColor();
    emittedHeaderGlobal = true;
  }
  printGlobalRecord(os, var, hasColors);
}

void APIPrinter::printEnumConstant(const EnumConstantRecord *constant) {
  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  - name: ";
  if (hasColors)
    os.resetColor();
  os << constant->name << "\n";
  printUSR(os, constant->usr, hasColors);
  printLocation(os, constant->loc, hasColors);
  printAvailability(os, constant->availability, hasColors);
}

void APIPrinter::visitEnum(const EnumRecord &var) {
  if (!emittedHeaderEnum) {
    if (hasColors)
      os.changeColor(raw_ostream::GREEN);
    os << "enums:\n";
    if (hasColors)
      os.resetColor();
    emittedHeaderEnum = true;
  }

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "- name: ";
  if (hasColors)
    os.resetColor();
  os << var.name << "\n";

  printUSR(os, var.usr, hasColors);
  printLocation(os, var.loc, hasColors);
  printAvailability(os, var.availability, hasColors);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  constants:\n";
  if (hasColors)
    os.resetColor();
  for (const auto *constant : var.constants)
    printEnumConstant(constant);
}

void APIPrinter::printStructField(const StructFieldRecord *field) {
  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  - name: ";
  if (hasColors)
    os.resetColor();
  os << field->name << "\n";
  printUSR(os, field->usr, hasColors);
  printLocation(os, field->loc, hasColors);
  printAvailability(os, field->availability, hasColors);
}

void APIPrinter::visitStruct(const StructRecord &s) {
  if (!emittedHeaderStruct) {
    if (hasColors)
      os.changeColor(raw_ostream::GREEN);
    os << "structs:\n";
    if (hasColors)
      os.resetColor();
    emittedHeaderStruct = true;
  }

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "- name: ";
  if (hasColors)
    os.resetColor();
  os << s.name << "\n";

  printUSR(os, s.usr, hasColors);
  printLocation(os, s.loc, hasColors);
  printAvailability(os, s.availability, hasColors);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  fields:\n";
  if (hasColors)
    os.resetColor();
  for (const auto *field : s.fields)
    printStructField(field);
}

void APIPrinter::printProtocol(SymbolInfo protocol) {
  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  - name: ";
  if (hasColors)
    os.resetColor();
  os << protocol.name << "\n";
  printUSR(os, protocol.usr, hasColors, /*indent=*/4);
}

void APIPrinter::printMethod(const ObjCMethodRecord *method) {
  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  - name: ";
  if (hasColors)
    os.resetColor();
  os << method->name << "\n";

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "    kind: ";
  if (hasColors)
    os.resetColor();
  os << (method->isInstanceMethod ? "instance" : "class") << "\n";

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "    isOptional: ";
  if (hasColors)
    os.resetColor();
  os << (method->isOptional ? "true" : "false") << "\n";

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "    isDynamic: ";
  if (hasColors)
    os.resetColor();
  os << (method->isDynamic ? "true" : "false") << "\n";

  printUSR(os, method->usr, hasColors);
  printLocation(os, method->loc, hasColors);
  printAvailability(os, method->availability, hasColors);
}

void APIPrinter::printProperty(const ObjCPropertyRecord *property) {
  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  - name: ";
  if (hasColors)
    os.resetColor();
  os << property->name << "\n";

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "    attributes:";
  if (hasColors)
    os.resetColor();
  if (property->isReadOnly())
    os << " readonly";
  if (property->isDynamic())
    os << " dynamic";
  if (property->isClassProperty())
    os << " class";
  os << "\n";

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "    isOptional: ";
  if (hasColors)
    os.resetColor();
  os << (property->isOptional ? "true" : "false") << "\n";

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "    getter name: ";
  if (hasColors)
    os.resetColor();
  os << property->getterName << "\n";

  if (!property->isReadOnly()) {
    if (hasColors)
      os.changeColor(raw_ostream::BLUE);
    os << "    setter name: ";
    if (hasColors)
      os.resetColor();
    os << property->setterName << "\n";
  }

  printUSR(os, property->usr, hasColors);
  printLocation(os, property->loc, hasColors);
  printAvailability(os, property->availability, hasColors);
}

void APIPrinter::printInstanceVariable(const ObjCInstanceVariableRecord *ivar) {
  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  - name: ";
  if (hasColors)
    os.resetColor();
  os << ivar->name << "\n";

  printUSR(os, ivar->usr, hasColors);
  printLocation(os, ivar->loc, hasColors);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "    access: ";
  if (hasColors)
    os.resetColor();
  switch (ivar->accessControl) {
  case ObjCInstanceVariableRecord::AccessControl::Private:
    os << "private\n";
    break;
  case ObjCInstanceVariableRecord::AccessControl::Protected:
    os << "protected\n";
    break;
  case ObjCInstanceVariableRecord::AccessControl::Public:
    os << "public\n";
    break;
  case ObjCInstanceVariableRecord::AccessControl::Package:
    os << "package\n";
    break;
  case ObjCInstanceVariableRecord::AccessControl::None:
    llvm_unreachable("cannonical acccess doesn't have None");
  }
  printLinkage(os, ivar->linkage, hasColors);
}

void APIPrinter::visitObjCInterface(const ObjCInterfaceRecord &interface) {
  if (!emittedHeaderInterface) {
    if (hasColors)
      os.changeColor(raw_ostream::GREEN);
    os << "objective-c interfaces:\n";
    if (hasColors)
      os.resetColor();
    emittedHeaderInterface = true;
  }

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "- name: ";
  if (hasColors)
    os.resetColor();
  os << interface.name << "\n";

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  superClassName: ";
  if (hasColors)
    os.resetColor();
  os << interface.superClass.name << "\n";

  if (!interface.superClass.usr.empty()) {
    if (hasColors)
      os.changeColor(raw_ostream::BLUE);
    os << "  superClassUSR: ";
    if (hasColors)
      os.resetColor();
    os << interface.superClass.usr << "\n";
  }

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  hasExceptionAttribute: ";
  if (hasColors)
    os.resetColor();
  os << (interface.hasExceptionAttribute ? "true" : "false") << "\n";

  printUSR(os, interface.usr, hasColors);
  printLocation(os, interface.loc, hasColors);
  printAvailability(os, interface.availability, hasColors);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  categories:";
  if (hasColors)
    os.resetColor();
  for (const auto *category : interface.categories)
    if (!category->name.empty())
      os << " " << category->name;
  os << "\n";

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  protocols:\n";
  if (hasColors)
    os.resetColor();
  for (const auto &protocol : interface.protocols)
    printProtocol(protocol);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  methods:\n";
  if (hasColors)
    os.resetColor();
  for (const auto *method : interface.methods)
    printMethod(method);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  properties:\n";
  if (hasColors)
    os.resetColor();
  for (const auto *property : interface.properties)
    printProperty(property);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  instance variables:\n";
  if (hasColors)
    os.resetColor();
  for (const auto *ivar : interface.ivars)
    printInstanceVariable(ivar);

  printLinkage(os, interface.linkage, hasColors);
}

void APIPrinter::visitObjCCategory(const ObjCCategoryRecord &category) {
  if (!emittedHeaderCategory) {
    if (hasColors)
      os.changeColor(raw_ostream::GREEN);
    os << "objective-c categories:\n";
    if (hasColors)
      os.resetColor();
    emittedHeaderCategory = true;
  }

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "- name: ";
  if (hasColors)
    os.resetColor();
  os << category.name << "\n";

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  interfaceName: ";
  if (hasColors)
    os.resetColor();
  os << category.interface.name << "\n";

  printUSR(os, category.usr, hasColors);
  printLocation(os, category.loc, hasColors);
  printAvailability(os, category.availability, hasColors);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  protocols:\n";
  if (hasColors)
    os.resetColor();
  for (const auto &protocol : category.protocols)
    printProtocol(protocol);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  methods:\n";
  if (hasColors)
    os.resetColor();
  for (const auto *method : category.methods)
    printMethod(method);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  properties:\n";
  if (hasColors)
    os.resetColor();
  for (const auto *property : category.properties)
    printProperty(property);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  instance variables:\n";
  if (hasColors)
    os.resetColor();
  for (const auto *ivar : category.ivars)
    printInstanceVariable(ivar);
}

void APIPrinter::visitObjCProtocol(const ObjCProtocolRecord &protocol) {
  if (!emittedHeaderProtocol) {
    if (hasColors)
      os.changeColor(raw_ostream::GREEN);
    os << "objective-c protocols:\n";
    if (hasColors)
      os.resetColor();
    emittedHeaderProtocol = true;
  }

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "- name: ";
  if (hasColors)
    os.resetColor();
  os << protocol.name << "\n";

  printUSR(os, protocol.usr, hasColors);
  printLocation(os, protocol.loc, hasColors);
  printAvailability(os, protocol.availability, hasColors);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  protocols:\n";
  if (hasColors)
    os.resetColor();
  for (const auto &protocol : protocol.protocols)
    printProtocol(protocol);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  methods:\n";
  if (hasColors)
    os.resetColor();
  for (const auto *method : protocol.methods)
    printMethod(method);

  if (hasColors)
    os.changeColor(raw_ostream::BLUE);
  os << "  properties:\n";
  if (hasColors)
    os.resetColor();
  for (const auto *property : protocol.properties)
    printProperty(property);
}

void APIPrinter::visitTypeDef(const TypedefRecord &type) {
  if (!emittedHeaderTypedef) {
    if (hasColors)
      os.changeColor(raw_ostream::GREEN);
    os << "type defs:\n";
    if (hasColors)
      os.resetColor();
    emittedHeaderTypedef = true;
  }
  printAPIRecord(os, type, hasColors);

  if (!type.underlyingType.usr.empty()) {
    if (hasColors)
      os.changeColor(raw_ostream::BLUE);
    os << "  type USR: ";
    if (hasColors)
      os.resetColor();
    os << type.underlyingType.usr << "\n";
  }
}

TAPI_NAMESPACE_INTERNAL_END
