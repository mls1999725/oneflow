"""
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
"""
from typing import Optional

import oneflow._oneflow_internal
from oneflow.compatible import single_client as flow
from oneflow.compatible.single_client.framework import id_util as id_util
from oneflow.compatible.single_client.framework import remote_blob as remote_blob_util


def eager_nccl_all_reduce(
    x: oneflow._oneflow_internal.BlobDesc,
    parallel_conf: str,
    name: Optional[str] = None,
) -> oneflow._oneflow_internal.BlobDesc:
    return (
        flow.user_op_builder(
            name if name is not None else id_util.UniqueStr("EagerNcclAllReduce_")
        )
        .Op("eager_nccl_all_reduce")
        .Input("in", [x])
        .Output("out")
        .Attr("parallel_conf", parallel_conf)
        .Build()
        .InferAndTryRun()
        .RemoteBlobList()[0]
    )
