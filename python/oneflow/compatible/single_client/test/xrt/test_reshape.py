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

import unittest

import numpy as np

import oneflow.compatible.single_client.unittest
from oneflow.compatible import single_client as flow

config = flow.function_config()


def make_job(x_shape, shape, dtype=flow.float32):
    config.use_xla_jit(False)
    config.use_tensorrt(False)

    @flow.global_function(config)
    def reshape_job(x=flow.FixedTensorDef(x_shape, dtype=dtype)):
        return flow.reshape(x, shape)

    return reshape_job


def make_xla_job(x_shape, shape, dtype=flow.float32):
    config.use_xla_jit(True)
    config.use_tensorrt(False)

    @flow.global_function(config)
    def xla_reshape_job(x=flow.FixedTensorDef(x_shape, dtype=dtype)):
        return flow.reshape(x, shape)

    return xla_reshape_job


def make_trt_job(x_shape, shape, dtype=flow.float32):
    config.use_xla_jit(False)
    config.use_tensorrt(True)

    @flow.global_function(config)
    def trt_reshape_job(x=flow.FixedTensorDef(x_shape, dtype=dtype)):
        return flow.reshape(x, shape)

    return trt_reshape_job


class TestReshape(unittest.TestCase):
    def _test_body(self, x, shape, dtype=np.float32):
        f1 = make_job(x.shape, shape, dtype=flow.float32)
        f2 = make_xla_job(x.shape, shape, dtype=flow.float32)
        a = f1(x).get()
        b = f2(x).get()
        print("without xla: ", a)
        print("with xla: ", b)
        self.assertTrue(a.shape == b.shape)
        self.assertTrue(np.allclose(a.numpy(), b.numpy(), rtol=0.001, atol=1e-05))
        flow.clear_default_session()
        f3 = make_trt_job(x.shape, shape, dtype=flow.float32)
        c = f3(x).get()
        print("with tensorrt: ", c)
        self.assertTrue(a.shape == c.shape)
        self.assertTrue(np.allclose(a.numpy(), c.numpy(), rtol=0.001, atol=1e-05))
        flow.clear_default_session()

    def _test_ones_body(self, x_shape, shape, dtype=np.float32):
        x = np.ones(x_shape, dtype=dtype)
        self._test_body(x, shape, dtype=dtype)

    def _test_random_body(self, x_shape, shape, dtype=np.float32):
        x = np.random.random(x_shape).astype(dtype)
        self._test_body(x, shape, dtype=dtype)

    def test_ones_input(self):
        self._test_ones_body((1, 10), (10,))
        self._test_ones_body((2, 10, 2), (4, 10))
        self._test_ones_body((2, 5, 2, 2), (2, 5, 4))

    def test_random_input(self):
        self._test_random_body((1, 10), (10,))
        self._test_random_body((2, 10, 2), (4, 10))
        self._test_random_body((2, 5, 2, 2), (2, 5, 4))


if __name__ == "__main__":
    unittest.main()
