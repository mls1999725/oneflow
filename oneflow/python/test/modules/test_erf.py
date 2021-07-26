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
import oneflow as flow
from scipy import special
from collections import OrderedDict
import oneflow as flow
from test_util import GenArgList

from automated_test_util import *

def _test_erf_impl(test_case, shape, device):
    np_input = np.random.randn(*shape)
    of_input = flow.Tensor(
        np_input, dtype=flow.float32, device=flow.device(device), requires_grad=True
    )

    of_out = flow.erf(of_input)
    np_out = special.erf(np_input)
    test_case.assertTrue(np.allclose(of_out.numpy(), np_out, 1e-5, 1e-5))

    of_out = of_out.sum()
    of_out.backward()
    test_case.assertTrue(
        np.allclose(
            of_input.grad.numpy(),
            (2 / np.sqrt(np.pi)) * np.exp(-np.square(of_input.numpy())),
            1e-5,
            1e-5,
        )
    )


def _test_tensor_erf_impl(test_case, shape, device):
    np_input = np.random.randn(*shape)
    of_input = flow.Tensor(
        np_input, dtype=flow.float32, device=flow.device(device), requires_grad=True
    )

    of_out = of_input.erf()
    np_out = special.erf(np_input)
    test_case.assertTrue(np.allclose(of_out.numpy(), np_out, 1e-5, 1e-5))

    of_out = of_out.sum()
    of_out.backward()
    test_case.assertTrue(
        np.allclose(
            of_input.grad.numpy(),
            (2 / np.sqrt(np.pi)) * np.exp(-np.square(of_input.numpy())),
            1e-5,
            1e-5,
        )
    )


@flow.unittest.skip_unless_1n1d()
class TestErfModule(flow.unittest.TestCase):
    def test_erf(test_case):
        arg_dict = OrderedDict()
        arg_dict["shape"] = [(2,), (2, 3), (2, 3, 4), (2, 4, 5, 6)]
        arg_dict["device"] = ["cpu", "cuda"]
        for arg in GenArgList(arg_dict):
            _test_erf_impl(test_case, *arg)
            _test_tensor_erf_impl(test_case, *arg)
    
    @autotest()
    def test_flow_tensor_erf_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        return x.erf()

    @autotest()
    def test_flow_erf_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor().to(device)
        y = torch.erf(x)
        return y


if __name__ == "__main__":
    unittest.main()
