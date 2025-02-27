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

import os
import unittest
from collections import OrderedDict

import numpy as np
import tensorflow as tf
from test_util import GenArgList

import oneflow._oneflow_internal
import oneflow.compatible.single_client.unittest
from oneflow.compatible import single_client as flow
from oneflow.compatible.single_client import typing as oft

gpus = tf.config.experimental.list_physical_devices("GPU")
for gpu in gpus:
    tf.config.experimental.set_memory_growth(gpu, True)


def reduce_sum_like(x, like, axis):
    name = "reduce_sum_like_op"
    op = (
        flow.user_op_builder(name)
        .Op("reduce_sum_like")
        .Input("x", [x])
        .Input("like", [like])
        .Output("y")
        .Attr("axis", axis)
        .Build()
    )
    return op.InferAndTryRun().SoleOutputBlob()


def compare_with_tensorflow(
    device_type, data_type, input_shape, axis, keepdims, rtol=1e-05, atol=1e-05
):
    assert device_type in ["gpu", "cpu"]
    flow.clear_default_session()
    func_config = flow.FunctionConfig()
    func_config.default_data_type(flow.float)

    @flow.global_function(function_config=func_config)
    def ReduceSumLikeJob(x: oft.Numpy.Placeholder(input_shape)):
        with flow.scope.placement(device_type, "0:0"):
            if data_type == "float16":
                x = flow.cast(x, dtype=flow.float16)
                like = flow.math.reduce_sum(x, axis=axis, keepdims=keepdims)
                y = reduce_sum_like(x, like, axis=axis)
                y = flow.cast(y, dtype=flow.float32)
            else:
                like = flow.math.reduce_sum(x, axis=axis, keepdims=keepdims)
                y = reduce_sum_like(x, like, axis=axis)
            return y

    x = np.random.rand(*input_shape).astype(np.float16).astype(np.float32)
    of_out = ReduceSumLikeJob(x).get()
    tf_out = tf.math.reduce_sum(x, axis=axis, keepdims=keepdims)
    if data_type == "float16":
        tf_out = tf.cast(tf_out, dtype=tf.float16)
        tf_out = tf.cast(tf_out, dtype=tf.float32)
    assert np.allclose(of_out.numpy(), tf_out.numpy(), rtol=rtol, atol=atol), (
        of_out.numpy(),
        tf_out.numpy(),
    )


@flow.unittest.skip_unless_1n2d()
class TestReduceSum(flow.unittest.TestCase):
    def test_reduce_sum(test_case):
        arg_dict = OrderedDict()
        arg_dict["device_type"] = ["gpu"]
        arg_dict["data_type"] = ["float16", "float32", "float16"]
        arg_dict["input_shape"] = [(2, 4, 8)]
        arg_dict["axis"] = [[], [0, 2]]
        arg_dict["keepdims"] = [True, False]
        for arg in GenArgList(arg_dict):
            compare_with_tensorflow(*arg)

    def test_col_reduce(test_case):
        arg_dict = OrderedDict()
        arg_dict["device_type"] = ["gpu"]
        arg_dict["data_type"] = ["float32", "float16"]
        arg_dict["input_shape"] = [(32, 2)]
        arg_dict["axis"] = [[0]]
        arg_dict["keepdims"] = [True, False]
        for arg in GenArgList(arg_dict):
            compare_with_tensorflow(*arg)

    def test_row_reduce(test_case):
        arg_dict = OrderedDict()
        arg_dict["device_type"] = ["gpu"]
        arg_dict["data_type"] = ["float32", "float16"]
        arg_dict["input_shape"] = [(2, 64)]
        arg_dict["axis"] = [[1]]
        arg_dict["keepdims"] = [True, False]
        for arg in GenArgList(arg_dict):
            compare_with_tensorflow(*arg)

    def test_scalar(test_case):
        arg_dict = OrderedDict()
        arg_dict["device_type"] = ["gpu"]
        arg_dict["data_type"] = ["float32", "float16"]
        arg_dict["input_shape"] = [(64, 2)]
        arg_dict["axis"] = [[0, 1]]
        arg_dict["keepdims"] = [True, False]
        for arg in GenArgList(arg_dict):
            compare_with_tensorflow(*arg)

    def test_split_axis_reduced(test_case):
        flow.config.gpu_device_num(2)
        func_config = flow.FunctionConfig()
        func_config.default_logical_view(flow.scope.consistent_view())

        @flow.global_function(function_config=func_config)
        def Foo(x: oft.Numpy.Placeholder((10,))):
            y = flow.math.reduce_sum(x)
            test_case.assertTrue(y.split_axis == flow.INVALID_SPLIT_AXIS)

        Foo(np.ndarray((10,), dtype=np.float32))


if __name__ == "__main__":
    unittest.main()
