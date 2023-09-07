//===- lib/Core/APIMutator.cpp - TAPI API Visitor ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "tapi/Core/APIVisitor.h"

using namespace llvm;

TAPI_NAMESPACE_INTERNAL_BEGIN

APIMutator::~APIMutator() {}

void APIMutator::visitMacroDefinition(MacroDefinitionRecord &) {}
void APIMutator::visitGlobal(GlobalRecord &) {}
void APIMutator::visitEnum(EnumRecord &) {}
void APIMutator::visitObjCInterface(ObjCInterfaceRecord &) {}
void APIMutator::visitObjCCategory(ObjCCategoryRecord &) {}
void APIMutator::visitObjCProtocol(ObjCProtocolRecord &) {}
void APIMutator::visitTypeDef(TypedefRecord &) {}
void APIMutator::visitStruct(StructRecord &) {}

APIVisitor::~APIVisitor() {}

void APIVisitor::visitMacroDefinition(const MacroDefinitionRecord &) {}
void APIVisitor::visitGlobal(const GlobalRecord &) {}
void APIVisitor::visitEnum(const EnumRecord &) {}
void APIVisitor::visitObjCInterface(const ObjCInterfaceRecord &) {}
void APIVisitor::visitObjCCategory(const ObjCCategoryRecord &) {}
void APIVisitor::visitObjCProtocol(const ObjCProtocolRecord &) {}
void APIVisitor::visitTypeDef(const TypedefRecord &) {}
void APIVisitor::visitStruct(const StructRecord &) {}

TAPI_NAMESPACE_INTERNAL_END
