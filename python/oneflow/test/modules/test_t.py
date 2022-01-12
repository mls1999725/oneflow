"""
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, softw
distributed under the License is distributed on an "AS IS" BASIS
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or 
See the License for the specific language governing permissions 
limitations under the License.
"""

import unittest
from collections import OrderedDict

import numpy as np

from oneflow.test_utils.automated_test_util import *
from test_util import GenArgList

import oneflow as flow
import oneflow.unittest


@flow.unittest.skip_unless_1n1d()
class TestTransposeAllDimFunction(flow.unittest.TestCase):
    @autotest(check_graph=True)
    def test_t_flow_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(ndim=random(0, 4)).to(device)
        y = torch.t(x)
        return y


if __name__ == "__main__":
    unittest.main()