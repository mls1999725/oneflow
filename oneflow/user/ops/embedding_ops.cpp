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
#include "oneflow/core/framework/framework.h"
#include "oneflow/core/operator/operator.h"
#include "oneflow/core/job/embedding.h"

namespace oneflow {

REGISTER_USER_OP("embedding_prefetch")
    .Input("indices")
    .Output("num_unique_indices")
    .Output("unique_indices")
    .Output("reverse_idx")
    .Attr<std::string>("name")
    .SetTensorDescInferFn([](user_op::InferContext* ctx) -> Maybe<void> {
      *ctx->OutputShape("num_unique_indices", 0) = Shape({1});
      *ctx->OutputShape("unique_indices", 0) = ctx->InputShape("indices", 0);
      *ctx->OutputShape("reverse_idx", 0) = ctx->InputShape("indices", 0);
      *ctx->OutputIsDynamic("num_unique_indices", 0) = false;
      *ctx->OutputIsDynamic("unique_indices", 0) = true;
      *ctx->OutputIsDynamic("reverse_idx", 0) = ctx->InputIsDynamic("indices", 0);
      return Maybe<void>::Ok();
    })
    .SetDataTypeInferFn([](user_op::InferContext* ctx) -> Maybe<void> {
      *ctx->OutputDType("reverse_idx", 0) = DataType::kInt32;
      *ctx->OutputDType("num_unique_indices", 0) = DataType::kInt32;
      *ctx->OutputDType("unique_indices", 0) = ctx->InputDType("indices", 0);
      return Maybe<void>::Ok();
    })
    .SetGetSbpFn(user_op::GetSbpFnUtil::DefaultBroadcastToBroadcast);

REGISTER_USER_OP("embedding_lookup")
    .Input("num_unique_indices")
    .Input("unique_indices")
    .Input("reverse_idx")
    .Output("embeddings")
    .Output("unique_values")
    .Attr<std::string>("name")
    .Attr<std::string>("optimizer")
    .Attr<int64_t>("embedding_size")
    .Attr<DataType>("dtype")
    .SetTensorDescInferFn([](user_op::InferContext* ctx) -> Maybe<void> {
      DimVector out_dim_vec = ctx->InputShape("unique_indices", 0).dim_vec();
      const int64_t embedding_vec_size = embedding::GetEmbeddingVecSize(
          ctx->Attr<int64_t>("embedding_size"), ctx->Attr<std::string>("optimizer"));
      out_dim_vec.push_back(embedding_vec_size);
      *ctx->OutputShape("embeddings", 0) = Shape(out_dim_vec);
      *ctx->OutputShape("unique_values", 0) = Shape(out_dim_vec);
      *ctx->OutputIsDynamic("embeddings", 0) = ctx->InputIsDynamic("reverse_idx", 0);
      *ctx->OutputIsDynamic("unique_values", 0) = true;
      return Maybe<void>::Ok();
    })
    .SetInputArgModifyFn([](user_op::GetInputArgModifier GetInputArgModifierFn,
                            const user_op::UserOpConfWrapper&) -> Maybe<void> {
      user_op::InputArgModifier* num_unique_indices =
          GetInputArgModifierFn("num_unique_indices", 0);
      CHECK_OR_RETURN(num_unique_indices != nullptr);
      num_unique_indices->set_requires_grad(false);
      user_op::InputArgModifier* unique_indices = GetInputArgModifierFn("unique_indices", 0);
      CHECK_OR_RETURN(unique_indices != nullptr);
      unique_indices->set_requires_grad(false);
      user_op::InputArgModifier* reverse_idx = GetInputArgModifierFn("reverse_idx", 0);
      CHECK_OR_RETURN(reverse_idx != nullptr);
      reverse_idx->set_requires_grad(false);
      return Maybe<void>::Ok();
    })
    .SetDataTypeInferFn([](user_op::InferContext* ctx) -> Maybe<void> {
      *ctx->OutputDType("embeddings", 0) = ctx->Attr<DataType>("dtype");
      *ctx->OutputDType("unique_values", 0) = ctx->Attr<DataType>("dtype");
      return Maybe<void>::Ok();
    })
    .SetGetSbpFn(user_op::GetSbpFnUtil::DefaultBroadcastToBroadcast);

REGISTER_USER_OP("sgd_embedding_update")
    .Input("num_unique_indices")
    .Input("unique_indices")
    .Input("reverse_idx")
    .Input("unique_values")
    .Input("embedding_diff")
    .OptionalInput("learning_rate")
    .OptionalInput("skip_if")
    .Attr<float>("learning_rate_val", 0.0)
    .Attr<float>("weight_decay", 0.0)
    .Attr<std::string>("name")
    .Attr<int64_t>("embedding_size")
    .SetTensorDescInferFn([](user_op::InferContext* ctx) -> Maybe<void> {
      return Maybe<void>::Ok();
    })
    .SetDataTypeInferFn([](user_op::InferContext* ctx) -> Maybe<void> { return Maybe<void>::Ok(); })
    .SetGetSbpFn(user_op::GetSbpFnUtil::DefaultBroadcastToBroadcast);

REGISTER_USER_OP_GRAD("embedding_lookup")
    .SetGenBackwardOpConfFn([](const user_op::UserOpWrapper& op,
                               user_op::AddOpFn AddOp) -> Maybe<void> {
      return Maybe<void>::Ok();
    });

}  // namespace oneflow
