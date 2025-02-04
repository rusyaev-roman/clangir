//====- LowerAttrToLLVMIR.cpp - Lowering CIR attributes to LLVMIR ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements lowering of CIR attributes to LLVMIR.
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/Target/LLVMIR/LLVMTranslationInterface.h"
#include "mlir/Target/LLVMIR/ModuleTranslation.h"
#include "clang/CIR/Dialect/IR/CIRAttrs.h"
#include "clang/CIR/Dialect/IR/CIRDialect.h"
#include "clang/CIR/Passes.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/GlobalVariable.h"

using namespace cir;
using namespace llvm;

namespace cir {
namespace direct {

/// Implementation of the dialect interface that converts CIR attributes to LLVM
/// IR metadata.
class CIRDialectLLVMIRTranslationInterface
    : public mlir::LLVMTranslationDialectInterface {
public:
  using LLVMTranslationDialectInterface::LLVMTranslationDialectInterface;

  /// Any named attribute in the CIR dialect, i.e, with name started with
  /// "cir.", will be handled here.
  mlir::LogicalResult
  amendOperation(mlir::Operation *op, mlir::NamedAttribute attribute,
                 mlir::LLVM::ModuleTranslation &moduleTranslation) const final {

    // Translate CIR's zero attribute to LLVM's zero initializer.
    if (isa<mlir::cir::ZeroAttr>(attribute.getValue())) {
      if (llvm::isa<mlir::LLVM::GlobalOp>(op)) {
        auto *globalVal = llvm::cast<llvm::GlobalVariable>(
            moduleTranslation.lookupGlobal(op));
        globalVal->setInitializer(
            llvm::Constant::getNullValue(globalVal->getValueType()));
      } else
        return op->emitError("#cir.zero not supported");
    }

    // Translate CIR's extra function attributes to LLVM's function attributes.
    auto func = dyn_cast<mlir::LLVM::LLVMFuncOp>(op);
    if (!func)
      return mlir::success();
    llvm::Function *llvmFunc = moduleTranslation.lookupFunction(func.getName());
    if (auto extraAttr = attribute.getValue()
                             .dyn_cast<mlir::cir::ExtraFuncAttributesAttr>()) {
      for (auto attr : extraAttr.getElements()) {
        if (auto inlineAttr = attr.getValue().dyn_cast<mlir::cir::InlineAttr>()) {
          if (inlineAttr.isNoInline())
            llvmFunc->addFnAttr(llvm::Attribute::NoInline);
          else if (inlineAttr.isAlwaysInline())
            llvmFunc->addFnAttr(llvm::Attribute::AlwaysInline);
          else if (inlineAttr.isInlineHint())
            llvmFunc->addFnAttr(llvm::Attribute::InlineHint);
          else
            llvm_unreachable("Unknown inline kind");
        }
      }
    }

    // Drop ammended CIR attribute from LLVM op.
    op->removeAttr(attribute.getName());
    return mlir::success();
  }
};

void registerCIRDialectTranslation(mlir::DialectRegistry &registry) {
  registry.insert<mlir::cir::CIRDialect>();
  registry.addExtension(+[](mlir::MLIRContext *ctx, mlir::cir::CIRDialect *dialect) {
    dialect->addInterfaces<CIRDialectLLVMIRTranslationInterface>();
  });
}

void registerCIRDialectTranslation(mlir::MLIRContext &context) {
  mlir::DialectRegistry registry;
  registerCIRDialectTranslation(registry);
  context.appendDialectRegistry(registry);
}
} // namespace direct
} // namespace cir
