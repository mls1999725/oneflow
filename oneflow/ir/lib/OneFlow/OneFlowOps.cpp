/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "OneFlow/OneFlowOps.h"
#include <iostream>
#include <string>
#include "OneFlow/OneFlowDialect.h"
#include "OneFlow/Passes.h"
#include "llvm/ADT/STLExtras.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/PatternMatch.h"
#include "llvm/ADT/StringSet.h"
#include "mlir/Support/LogicalResult.h"

using namespace mlir;
using namespace mlir::oneflow;

static mlir::ParseResult parseConstantOp(mlir::OpAsmParser& parser, mlir::OperationState& result) {
  mlir::DenseElementsAttr value;
  if (parser.parseOptionalAttrDict(result.attributes)
      || parser.parseAttribute(value, "value", result.attributes)) {
    return failure();
  }
  result.addTypes(value.getType());
  return success();
}

static mlir::LogicalResult verify(oneflow::ConstantOp op) { return mlir::success(); }

template<typename OpType>
LogicalResult TrimRedundantCtrl(OpType& op, PatternRewriter& rewriter) {
  if (op.ctrl_output() && op.ctrl_output().use_empty()) {
    const int32_t num_data_outputs =
        *(op.result_segment_sizes().template getValues<uint32_t>()).begin();
    NamedAttrList attributes(op->getAttrDictionary());
    attributes.erase("result_segment_sizes");
    attributes.append("result_segment_sizes", rewriter.getI32VectorAttr({num_data_outputs, 0}));
    if (auto created =
            rewriter.create<OpType>(op->getLoc(), op.getODSResults(0 /* data out */).getTypes(),
                                    op->getOperands(), attributes)) {
      for (auto out : op.data_output()) {
        out.replaceAllUsesWith(created->getResult(out.getResultNumber()));
      }
      op->erase();
      return success();
    }
  }
  return failure();
}

bool IsCtrlOutTrimmed(oneflow::UserOp& op) { return !op.ctrl_output(); }

bool IsCtrlInAbsent(oneflow::UserOp& op) { return op.ctrl_inputs().empty(); }

StringSet<>* GetPrintedOpTypeNames() {
  static llvm::StringSet<> names({});
  return &names;
}

const StringSet<>& GetUnaryOpTypeNames() {
  static llvm::StringSet<> names({"abs", "acos", "ceil", "cosh", "floor", "lgamma", "log_sigmoid",
                                  "reciprocal_no_nan", "rint", "round", "softplus"

  });
  return names;
}

const StringSet<>& GetScalarMathOpTypeNames() {
  static llvm::StringSet<> names(
      {"scalar_add", "scalar_floordiv", "scalar_fmod", "scalar_mul", "scalar_pow"

      });
  return names;
}

const StringSet<>& GetReduceOpTypeNames() {
  static llvm::StringSet<> names(
      {"reduce_min", "reduce_prod", "reduce_sum", "reduce_max"

      });
  return names;
}

const StringSet<>& GetPoolOpTypeNames() {
  static llvm::StringSet<> names({"avgpool_1d", "avgpool_2d", "avgpool_3d", "avg_pool_1d",
                                  "avg_pool_2d", "avg_pool_3d", "max_pool_1d", "max_pool_2d",
                                  "max_pool_3d"

  });
  return names;
}

const StringSet<>& GetFloatUnaryOpTypeNames() {
  static llvm::StringSet<> names({"acosh", "asin",     "asinh",      "atan",  "atanh",      "sin",
                                  "cos",   "erf",      "erfc",       "exp",   "expm1",      "log",
                                  "log1p", "negative", "reciprocal", "rsqrt", "sigmoid_v2", "sign",
                                  "sinh",  "sqrt",     "square",     "tan",   "tanh"});
  return names;
}

struct ConcreteUserOps : public mlir::OpRewritePattern<oneflow::UserOp> {
  explicit ConcreteUserOps(mlir::MLIRContext* context)
      : OpRewritePattern<oneflow::UserOp>(context, /*benefit=*/1) {}
  mlir::LogicalResult matchAndRewrite(oneflow::UserOp op,
                                      mlir::PatternRewriter& rewriter) const override {
    auto op_type_name = op->getAttrOfType<StringAttr>("op_type_name").getValue();
    op.getODSResults(0);
    if (succeeded(TrimRedundantCtrl(op, rewriter))) {
      return success();
    }
    // In principle, a concrete user op has no ctrl input/output. Some benefits:
    // 1. simplify things
    // 2. make conversion and code gen more doable
    // 3. enable the reuse of established MLIR infra like built-in traits
    else if (IsCtrlOutTrimmed(op) && IsCtrlInAbsent(op)) {
      if (op_type_name.equals("relu") || op_type_name.equals("gelu") || op_type_name.equals("cast")
          || GetUnaryOpTypeNames().contains(op_type_name)
          || GetFloatUnaryOpTypeNames().contains(op_type_name)
          || GetScalarMathOpTypeNames().contains(op_type_name)
          || GetPoolOpTypeNames().contains(op_type_name)
          || GetReduceOpTypeNames().contains(op_type_name)
          || op_type_name.equals("reshape")) {
        NamedAttrList attributes(op->getAttrDictionary());
        attributes.erase("operand_segment_sizes");
        attributes.erase("result_segment_sizes");
        OperationState state(op->getLoc(), "oneflow." + op_type_name.str());
        state.addAttributes(attributes);
        state.addOperands(op->getOperands());
        assert(op.data_input().size() == 1);
        assert(op.data_output().size() == 1);
        state.addTypes(op.getODSResults(0 /* data out */).getTypes());
        if (auto elementwise = rewriter.createOperation(state)) {
          op.data_output().front().replaceAllUsesWith(elementwise->getResult(0));
          op->erase();
          return success();
        }
      } else if (op_type_name.equals("scalar_mul_by_tensor") || op_type_name.equals("matmul")
                 || op_type_name.equals("gather") || op_type_name.equals("gelu_grad")
                 || op_type_name.equals("conv2d") || op_type_name.equals("bias_add")) {
        assert(op.data_input().size() == 2);
        assert(op.data_output().size() == 1);
        NamedAttrList attributes(op->getAttrDictionary());
        attributes.erase("operand_segment_sizes");
        attributes.erase("result_segment_sizes");
        OperationState state(op->getLoc(), "oneflow." + op_type_name.str());
        state.addAttributes(attributes);
        state.addOperands(op.data_input());
        state.addTypes(op.getODSResults(0 /* data out */).getTypes());
        if (auto created = rewriter.createOperation(state)) {
          op.data_output().front().replaceAllUsesWith(created->getResult(0));
          op->erase();
          return success();
        }
      } else {
        if (!GetPrintedOpTypeNames()->contains(op.op_type_name())) {
          llvm::errs() << "MLIR opaque user op: " << op.op_type_name() << "\n";
          GetPrintedOpTypeNames()->insert(op.op_type_name());
        }
      }
    }
    return failure();
  }
};

void UserOp::getCanonicalizationPatterns(::mlir::RewritePatternSet& results,
                                         ::mlir::MLIRContext* context) {
  results.insert<ConcreteUserOps>(context);
}

struct ConcreteSystemOps : public mlir::OpRewritePattern<oneflow::SystemOp> {
  explicit ConcreteSystemOps(mlir::MLIRContext* context)
      : OpRewritePattern<oneflow::SystemOp>(context, /*benefit=*/1) {}
  mlir::LogicalResult matchAndRewrite(oneflow::SystemOp op,
                                      mlir::PatternRewriter& rewriter) const override {
    return TrimRedundantCtrl(op, rewriter);
  }
};

void SystemOp::getCanonicalizationPatterns(::mlir::RewritePatternSet& results,
                                           ::mlir::MLIRContext* context) {
  results.insert<ConcreteSystemOps>(context);
}

// TODO: merge all ctrl input and output when folding op
bool HaveIdenticalPlacement(mlir::Operation* a, mlir::Operation* b) {
  UserOpAdaptor adaptor_a(a->getOperands(), a->getAttrDictionary());
  UserOpAdaptor adaptor_b(b->getOperands(), b->getAttrDictionary());
  return adaptor_a.device_tag() == adaptor_b.device_tag()
         && adaptor_a.device_name() == adaptor_b.device_name();
}

OpFoldResult OpTrait::impl::foldIdempotentOfIdenticalPlacement(Operation* op) {
  auto* argument_op = op->getOperand(0).getDefiningOp();
  if (argument_op && op->getName() == argument_op->getName()
      && HaveIdenticalPlacement(op, argument_op)) {
    return op->getOperand(0);
  }
  return {};
}

OpFoldResult OpTrait::impl::foldInvolutionOfIdenticalPlacement(Operation* op) {
  auto* argument_op = op->getOperand(0).getDefiningOp();
  if (argument_op && op->getName() == argument_op->getName()
      && HaveIdenticalPlacement(op, argument_op)) {
    return argument_op->getOperand(0);
  }
  return {};
}

#include "OneFlow/OneFlowEnums.cpp.inc"

#define GET_OP_CLASSES
#include "OneFlow/OneFlowOps.cpp.inc"
