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
#include <string>
#include <vector>
#include "glog/logging.h"

#include "oneflow/core/operator/op_conf.pb.h"
#include "oneflow/xrt/api.h"
#include "oneflow/xrt/argument.h"
#include "oneflow/xrt/graph/graph.h"
#include "oneflow/xrt/passes/pass.h"
#include "oneflow/xrt/utility/stl.h"

namespace oneflow {
namespace xrt {

namespace shape_inference {

void InferShape(XrtGraph* graph, const XrtPassOptions& options, const ParallelContext* parallel_ctx,
                const ParallelDesc* parallel_desc,
                const util::PbMap<std::string, cfg::SbpSignature>* sbp_signatures,
                const util::PbMap<std::string, BlobDescProto>* lbn2logical_blob_desc_proto,
                util::Map<std::string, BlobDesc>* blob_descs) {
  algorithm::TopologyVisit(*graph, [&](XrtNode* node) {
    if (!node->IsArgumentNode()) {
      DeviceType device_type = XrtDeviceToDeviceType(node->device());
      const auto& conf = *dynamic_cast<const OperatorConf*>(&node->param());
      auto op = CHECK_JUST(ConstructOp(conf, device_type));
      CHECK_JUST(op->FillOpParallelDesc(*parallel_desc));
      auto get_blob_desc_fn = [&](const std::string& bn) -> BlobDesc* {
        const LogicalBlobId& lbi = op->BnInOp2Lbi(bn);
        std::string blob_name = BlobIdToName(lbi);
        auto it = blob_descs->find(blob_name);
        if (it == blob_descs->end()) {
          it = blob_descs->emplace(blob_name, BlobDesc(DataType::kInvalidDataType)).first;
        }
        return &(it->second);
      };

      util::Map<std::string, std::unique_ptr<BlobDesc>> lbn2logical_blob_desc;
      for (const auto& pair : *lbn2logical_blob_desc_proto) {
        lbn2logical_blob_desc[pair.first].reset(new BlobDesc(pair.second));
      }
      auto GetLogicalBlobDesc4BnInOp = [&](const std::string& bn) -> BlobDesc* {
        const LogicalBlobId& lbi = op->BnInOp2Lbi(bn);
        std::string blob_name = BlobIdToName(lbi);
        auto it = lbn2logical_blob_desc.find(blob_name);
        CHECK(it != lbn2logical_blob_desc.end());
        return it->second.get();
      };
      CHECK_JUST(op->FillLogicalInBlobDesc(GetLogicalBlobDesc4BnInOp));
      CHECK_JUST(op->FillLogicalOutBlobDesc(GetLogicalBlobDesc4BnInOp));
      const cfg::SbpSignature& sbp_signature = sbp_signatures->at(node->name());
      CHECK_JUST(op->FillSbpSignature(sbp_signature));
      CHECK_JUST(op->InferOutBlobDescsIf(get_blob_desc_fn, parallel_ctx));
    }
    // Update blob desc on the output edges.
    for (XrtEdge* edge : node->out_edges()) {
      std::string name = edge->argument().name();
      auto it = blob_descs->find(name);
      CHECK(it != blob_descs->end());
      const auto& metadata = edge->argument().meta_data();
      Argument argument(name, it->second.shape(), it->second.data_type(), metadata);
      edge->SetArgument(argument);
    }
  });
}

}  // namespace shape_inference

class InferShapePass : public XrtPass {
 public:
  InferShapePass() = default;

  void Run(XrtGraph* graph, const XrtPassOptions& options,
           const std::vector<Any>& params) override {
    CHECK_EQ(params.size(), 5) << "BlobDesc, ParallelCtx, ParallelDesc, SbpSignatures and "
                                  "lbn2logical_blob_desc_proto are required in "
                                  "InferShapePass.";
    const auto* parallel_ctx = any_cast<const ParallelContext*>(params[0]);
    const auto* parallel_desc = any_cast<const ParallelDesc*>(params[1]);
    const auto* sbp_signatures =
        any_cast<const util::PbMap<std::string, cfg::SbpSignature>*>(params[2]);
    const auto* lbn2logical_blob_desc_proto =
        any_cast<const util::PbMap<std::string, BlobDescProto>*>(params[3]);
    auto* blob_descs = any_cast<util::Map<std::string, BlobDesc>*>(params[4]);

    shape_inference::InferShape(graph, options, parallel_ctx, parallel_desc, sbp_signatures,
                                lbn2logical_blob_desc_proto, blob_descs);
  }
};

REGISTER_XRT_PASS(InferShape, InferShapePass);

}  // namespace xrt
}  // namespace oneflow
