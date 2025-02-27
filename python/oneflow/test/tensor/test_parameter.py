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
from collections import OrderedDict

import numpy as np
import oneflow as flow
import oneflow.unittest
from oneflow.test_utils.automated_test_util import *


@flow.unittest.skip_unless_1n1d()
class TestParameter(flow.unittest.TestCase):
    @autotest(n=1, check_graph=False)
    def test_parameter_grad_fn_none(test_case):
        x = torch.ones(2, 3).requires_grad_(True)
        y = x + x
        z = torch.nn.Parameter(y)
        return z.grad_fn

    @autotest(n=1, check_graph=False)
    def test_parameter_set_data_autograd_meta(test_case):
        x = torch.ones(2, 3).requires_grad_(True)
        y = x + x
        z = torch.nn.Parameter(x)
        z.data = y
        return z.grad_fn, z.is_leaf

    def test_parameter_set_data(test_case):
        a = flow.nn.Parameter(flow.ones(2, 3), False)
        old_id = id(a)
        b = flow.nn.Parameter(flow.ones(4, 5), True)
        a.data = b
        test_case.assertEqual(old_id, id(a))
        test_case.assertTrue(a.shape == (4, 5))
        test_case.assertFalse(a.requires_grad)
        test_case.assertTrue(a.is_leaf)


if __name__ == "__main__":
    unittest.main()
