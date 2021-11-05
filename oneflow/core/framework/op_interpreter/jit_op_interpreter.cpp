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
#include "mlir/Dialect/StandardOps/IR/Ops.h"
#ifdef WITH_MLIR
#include "oneflow/core/common/maybe.h"
#include "oneflow/core/framework/op_expr.h"
#include "oneflow/core/framework/op_builder.h"
#include "oneflow/core/framework/multi_client_session_context.h"
#include "oneflow/core/framework/op_interpreter/jit_op_interpreter.h"
#include "oneflow/core/framework/op_interpreter/op_interpreter_util.h"
#include "oneflow/core/framework/instructions_builder.h"
#include "oneflow/core/framework/op_arg_util.h"
#include "oneflow/core/framework/scope_util.h"
#include "oneflow/core/framework/session_util.h"
#include "oneflow/core/framework/symbol_storage_util.h"
#include "oneflow/core/framework/tensor.h"
#include "oneflow/core/framework/tensor_name_scope.h"
#include "oneflow/core/framework/tensor_tuple.h"
#include "oneflow/core/framework/nd_sbp.h"
#include "oneflow/core/eager/foreign_boxing_util.h"
#include "oneflow/core/operator/operator.h"
#include "oneflow/core/job/job_build_and_infer_ctx_mgr.h"
#include "oneflow/core/vm/vm_util.h"

namespace oneflow {

namespace one {

using namespace mlir;

Maybe<void> JitInterpreter::Apply(const OpExpr& op_expr, const TensorTuple& inputs,
                                  TensorTuple* outputs, const OpExprInterpContext& ctx) const {
#define APPLY_IF(op_type)                                              \
  if (const auto* op = dynamic_cast<const op_type##Expr*>(&op_expr)) { \
    return ApplyImpl(*op, inputs, outputs, ctx);                       \
  }
  APPLY_IF(UserOp);
#undef APPLY_IF

  OF_UNIMPLEMENTED() << "The type " << op_expr.op_type_name()
                     << " has not been supported in LazyInterpreter::Apply.";
  return Maybe<void>::Ok();
}

std::string GetDeviceTag(const std::shared_ptr<Tensor>& tensor) {
  if (tensor->is_cuda()) {
    return "gpu";
  } else {
    return "cpu";
  }
}

Maybe<const ParallelDesc> GetParallelDesc(const std::shared_ptr<Tensor>& tensor) {
  if (tensor->is_local()) {
    const auto& device = JUST(tensor->device());
    const auto& placement = JUST(Placement4Device(device));
    return placement.shared_from_symbol();
  } else {
    return JUST(tensor->parallel_desc()).shared_from_symbol();
  }
}

void InsertLbnSegmentIntoVec(const ::mlir::ArrayAttr& lbn_segment_keys,
                             const ::mlir::ArrayAttr& lbn_segment_sizes,
                             std::vector<std::string>& indexed_bns) {
  for (const auto& bn_size_pair : llvm::zip(lbn_segment_keys, lbn_segment_sizes)) {
    const auto& bn = std::get<0>(bn_size_pair).dyn_cast<StringAttr>().getValue().str();
    const auto& length = std::get<1>(bn_size_pair).dyn_cast<IntegerAttr>().getInt();
    for (size_t i = 0; i < length; i++) {
      const auto indexed_bn = bn + "_" + std::to_string(i);
      indexed_bns.push_back(indexed_bn);
    }
  }
}

void JitInterpreter::Interrupt() {
  CHECK(importer_.LowerToOneFlowKernel().succeeded());
  if (ParseBooleanFromEnv("ONEFLOW_MLIR_STDOUT", false)) { module_->print(llvm::outs()); }
  llvm::DenseMap<Value, std::shared_ptr<Tensor>> mapping;
  // TODO: handle the case if there are more than one function in the module.
  ReturnOp return_op;
  const bool was_interrupted =
      module_
          ->walk([&](mlir::Operation* op) {
            if (llvm::dyn_cast<mlir::oneflow::UserOp>(op) || op->hasAttr("op_type_name")) {
              ::oneflow::OperatorConf op_conf;
              mlir::oneflow::UserOpAdaptor user_op_adaptor(op->getOperands(),
                                                           op->getAttrDictionary());
              const std::string op_name = user_op_adaptor.op_name().getValue().str();
              auto user_conf = op_conf.mutable_user_conf();
              if (succeeded(ConvertUserOpInputs(op, user_op_adaptor, user_conf))
                  && succeeded(ConvertUserOpOutputs(op, user_op_adaptor, user_conf))
                  && succeeded(importer_.ConvertUserOpAttributes(op, user_op_adaptor, op_conf))
                  && succeeded(ConvertCtrlInputs(op, op_conf))) {
                std::vector<std::string> indexed_ibns{};
                std::vector<std::string> indexed_obns{};
                InsertLbnSegmentIntoVec(user_op_adaptor.input_lbn_segment_keys(),
                                        user_op_adaptor.input_lbn_segment_sizes(), indexed_ibns);
                InsertLbnSegmentIntoVec(user_op_adaptor.output_lbn_segment_keys(),
                                        user_op_adaptor.output_lbn_segment_sizes(), indexed_obns);
                auto expr =
                    CHECK_JUST(UserOpExpr::New(user_op_adaptor.op_name().getValue().str(),
                                               std::move(*user_conf), indexed_ibns, indexed_obns));
                TensorTuple inputs(op->getOperands().size());
                for (auto indexed_operand : llvm::enumerate(op->getOperands())) {
                  auto index = indexed_operand.index();
                  auto operand = indexed_operand.value();
                  if (auto arg = operand.dyn_cast<mlir::BlockArgument>()) {
                    inputs[index] = GetJitForwardArgs()[arg.getArgNumber()];
                  } else {
                    auto found = mapping.find(operand);
                    if (found->first) {
                      inputs[index] = found->second;
                    } else {
                      operand.dump();
                      LOG(FATAL) << "tensor not found";
                    }
                  }
                }
                auto outputs = CHECK_JUST(OpInterpUtil::Dispatch<TensorTuple>(*expr, inputs));
                CHECK_EQ(outputs->size(), op->getResults().size());
                for (auto output_pair : llvm::zip(*outputs, op->getResults())) {
                  auto output_tensor = std::get<0>(output_pair);
                  Value output_result = std::get<1>(output_pair);
                  CHECK(mapping.insert({output_result, output_tensor}).second);
                }
                return WalkResult::advance();
              } else {
                return WalkResult::interrupt();
              }
            } else if (auto return_op_ = llvm::dyn_cast<ReturnOp>(op)) {
              return_op = return_op_;
              return WalkResult::advance();
            } else {
              return WalkResult::advance();
            }
          })
          .wasInterrupted();
  CHECK(!was_interrupted) << "JIT dispatch failure";
  for (auto indexed_return_value : llvm::enumerate(return_op->getOperands())) {
    auto value = indexed_return_value.value();
    auto found = mapping.find(value);
    CHECK(found != mapping.end()) << "tensor not found";
    importer_.GetReturnTensors()[indexed_return_value.index()]->ResetTensor(mapping[value]);
  }
}

Maybe<void> JitInterpreter::ApplyImpl(const UserOpExpr& op_expr, const TensorTuple& inputs,
                                      TensorTuple* outputs, const OpExprInterpContext& ctx) const {
  auto op_conf = JUST(OpInterpUtil::GenBuiltinOpConf(op_expr, ctx.attrs));
  const std::string device_tag = GetDeviceTag(inputs.at(0));
  const bool is_local = inputs.at(0)->is_local();
  const std::shared_ptr<const ParallelDesc> parallel_desc = JUST(GetParallelDesc(inputs.at(0)));
  importer_.SetParallelDesc(parallel_desc);
  op_conf->set_device_tag(device_tag);

  for (int i = 0; i < inputs.size(); ++i) {
    const auto& input_tensor = inputs.at(i);
    CHECK_OR_RETURN(device_tag == GetDeviceTag(input_tensor));
    CHECK_OR_RETURN(parallel_desc->EqualsIgnoringHierarchy(*JUST(GetParallelDesc(input_tensor))));
    CHECK_EQ_OR_RETURN(is_local, input_tensor->is_local());
  }
  CHECK_EQ_OR_RETURN(outputs->size(), op_expr.output_size());
  auto indexed_arg_name_and_index = op_expr.input_arg_tuple()->indexed_arg_name_and_index();
  CHECK_EQ_OR_RETURN(indexed_arg_name_and_index.size(), inputs.size());
  importer_.GetOrInsertFunc(GetJitFuncName(), inputs, outputs);
  importer_.CreateOperandMapping(*op_conf, parallel_desc, op_expr.input_arg_tuple(), inputs);
  CHECK_OR_RETURN(importer_.ProcessUserOp(*op_conf).succeeded());
  return Maybe<void>::Ok();
}

}  // namespace one

}  // namespace oneflow

#endif  // WITH_MLIR
