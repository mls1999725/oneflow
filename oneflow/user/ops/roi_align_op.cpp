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
#include "oneflow/core/framework/op_generated.h"

namespace oneflow {

/*static*/ Maybe<void> RoiAlignOp::GetSbp(user_op::SbpContext* ctx) {
  ctx->NewBuilder()
      .Split(user_op::OpArg("x", 0), 0)
      .Split(user_op::OpArg("rois", 0), 0)
      .Split(user_op::OpArg("y", 0), 0)
      .Build();
  return Maybe<void>::Ok();
}
/*static*/ Maybe<void> RoiAlignOp::InferLogicalTensorDesc(user_op::InferContext* ctx) {
  const Shape& x_shape = ctx->InputShape("x", 0);
  const Shape& rois_shape = ctx->InputShape("rois", 0);
  const int32_t pooled_h = ctx->Attr<int32_t>("pooled_h");
  const int32_t pooled_w = ctx->Attr<int32_t>("pooled_w");
  // x: feature map (N, C, H, W)
  CHECK_EQ(x_shape.NumAxes(), 4);
  // rois: (R, 5)
  CHECK_EQ(rois_shape.NumAxes(), 2);
  CHECK_EQ(rois_shape.At(1), 5);
  // y: (R, C, pool_h, pool_w)
  *ctx->OutputShape("y", 0) = Shape({rois_shape.At(0), x_shape.At(1), pooled_h, pooled_w});
  return Maybe<void>::Ok();
}
/*static*/ Maybe<void> RoiAlignOp::InferPhysicalTensorDesc(user_op::InferContext* ctx) {
  return InferLogicalTensorDesc(ctx);
}
/*static*/ Maybe<void> RoiAlignOp::InferDataType(user_op::InferContext* ctx) {
  *ctx->OutputDType("y", 0) = ctx->InputDType("x", 0);
  return Maybe<void>::Ok();
}
/*static*/ Maybe<void> RoiAlignOp::ModifyInputArg(const GetInputArgModifier& GetInputArgModifierFn,
                                                  const user_op::UserOpConfWrapper&) {
  user_op::InputArgModifier* roi_modifier = GetInputArgModifierFn("rois", 0);
  CHECK(roi_modifier != nullptr);
  roi_modifier->set_requires_grad(false);
  user_op::InputArgModifier* feat_modifier = GetInputArgModifierFn("x", 0);
  CHECK(feat_modifier != nullptr);
  feat_modifier->set_requires_grad(true);
  return Maybe<void>::Ok();
}

/*static*/ Maybe<void> RoiAlignGradOp::GetSbp(user_op::SbpContext* ctx) {
  ctx->NewBuilder()
      .Split(user_op::OpArg("dy", 0), 0)
      .Split(user_op::OpArg("x_like", 0), 0)
      .Split(user_op::OpArg("rois", 0), 0)
      .Split(user_op::OpArg("dx", 0), 0)
      .Build();
  return Maybe<void>::Ok();
}
/*static*/ Maybe<void> RoiAlignGradOp::InferLogicalTensorDesc(user_op::InferContext* ctx) {
  const Shape& dy_shape = ctx->InputShape("dy", 0);
  const Shape& x_like_shape = ctx->InputShape("x_like", 0);
  const Shape& rois_shape = ctx->InputShape("rois", 0);
  const int32_t pooled_h = ctx->Attr<int32_t>("pooled_h");
  const int32_t pooled_w = ctx->Attr<int32_t>("pooled_w");
  // x: feature map (N, C, H, W)
  CHECK_EQ_OR_RETURN(x_like_shape.NumAxes(), 4);
  // rois: (R, 5)
  CHECK_EQ_OR_RETURN(rois_shape.NumAxes(), 2);
  CHECK_EQ_OR_RETURN(rois_shape.At(1), 5);
  // y: (R, C, pool_h, pool_w)
  const Shape& y_shape = Shape({rois_shape.At(0), x_like_shape.At(1), pooled_h, pooled_w});
  CHECK_EQ_OR_RETURN(y_shape, dy_shape);
  *ctx->OutputShape("dx", 0) = x_like_shape;
  return Maybe<void>::Ok();
}
/*static*/ Maybe<void> RoiAlignGradOp::InferPhysicalTensorDesc(user_op::InferContext* ctx) {
  return InferLogicalTensorDesc(ctx);
}
/*static*/ Maybe<void> RoiAlignGradOp::InferDataType(user_op::InferContext* ctx) {
  CHECK_EQ_OR_RETURN(ctx->InputDType("dy", 0), ctx->InputDType("x_like", 0));
  *ctx->OutputDType("dx", 0) = ctx->InputDType("x_like", 0);
  return Maybe<void>::Ok();
}

namespace {

Maybe<void> GenerateBackwardOpConf4RoiAlign(const user_op::UserOpWrapper& op,
                                            const user_op::AddOpFn& AddOp) {
  if (op.NeedGenGradTensor4OpInput("x", 0)) {
    user_op::UserOpConfWrapperBuilder builder(op.op_name() + "_grad");
    user_op::UserOpConfWrapper grad_op =
        builder.Op("roi_align_grad")
            .Input("dy", op.GetGradTensorWithOpOutput("y", 0))
            .Input("x_like", op.input("x", 0))
            .Input("rois", op.input("rois", 0))
            .Attr("pooled_h", op.attr<int32_t>("pooled_h"))
            .Attr("pooled_w", op.attr<int32_t>("pooled_w"))
            .Attr("spatial_scale", op.attr<float>("spatial_scale"))
            .Attr("sampling_ratio", op.attr<int32_t>("sampling_ratio"))
            .Attr("aligned", op.attr<bool>("aligned"))
            .Output("dx")
            .Build();
    op.BindGradTensorWithOpInput(grad_op.output("dx", 0), "x", 0);
    AddOp(grad_op);
  }
  return Maybe<void>::Ok();
}

}  // namespace

REGISTER_USER_OP_GRAD("roi_align").SetGenBackwardOpConfFn(GenerateBackwardOpConf4RoiAlign);

}  // namespace oneflow
