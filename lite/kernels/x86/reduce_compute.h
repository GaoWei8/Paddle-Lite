// Copyright (c) 2019 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <vector>
#include "lite/core/kernel.h"
#include "lite/core/op_registry.h"
#include "lite/fluid/eigen.h"
#include "lite/kernels/x86/reduce_op_function.h"

namespace paddle {
namespace lite {
namespace kernels {
namespace x86 {

struct SumFunctor {
  template <typename X, typename Y, typename Dim>
  void operator()(X* x, Y* y, const Dim& dim, size_t d, size_t r_d) {
    for (int i = 0; i < dim[0]; i++) {
      for (int k = 0; k < dim[2]; k++) {
        auto output_temp = x[i * dim[1] * dim[2] + k];
        for (int j = 1; j < dim[1]; j++) {
          int input_d = i * dim[1] * dim[2] + j * dim[2] + k;
          output_temp = output_temp + x[input_d];
        }
        y[i * dim[2] + k] = output_temp;
      }
    }
  }

  template <typename X, typename Y, typename Dim>
  void operator()(X* x, Y* y, const Dim& dim) {
    y->device(lite::fluid::EigenDeviceType<TARGET(kX86)>()) = x->sum(dim);
  }
};

#define HANDLE_DIM(NDIM, RDIM)                                            \
  if (ndim == NDIM && rdim == RDIM) {                                     \
    paddle::lite::kernels::x86::                                          \
        ReduceFunctor<lite::TargetType::kX86, T, NDIM, RDIM, SumFunctor>( \
            *input, output, dims, keep_dim);                              \
  }

#define HANDLE_DIMT(NDIM, RDIM)                                             \
  if (ndim == NDIM && rdim == RDIM) {                                       \
    paddle::lite::kernels::x86::ReduceFunctorTensor<lite::TargetType::kX86, \
                                                    T,                      \
                                                    NDIM,                   \
                                                    RDIM,                   \
                                                    SumFunctor>(            \
        *input, output, dims, keep_dim);                                    \
  }

template <typename T>
class ReduceSumCompute : public KernelLite<TARGET(kX86), PRECISION(kFloat)> {
 public:
  using param_t = operators::ReduceParam;

  void Run() override {
    auto& param = *param_.get_mutable<operators::ReduceParam>();
    // auto& context = ctx_->As<X86Context>();
    bool reduce_all = param.reduce_all;
    auto* input = param.x;
    auto* output = param.output;
    param.output->mutable_data<T>();

    const auto& dims = param.dim;
    bool keep_dim = param.keep_dim;
    if (reduce_all) {
      // Flatten and reduce 1-D tensor
      auto x = lite::fluid::EigenVector<T>::Flatten(*input);
      auto out = lite::fluid::EigenScalar<T>::From(output);
      // auto& place = *platform::CPUDeviceContext().eigen_device();
      auto reduce_dim = Eigen::array<int, 1>({{0}});
      SumFunctor functor;
      functor(&x, &out, reduce_dim);
    } else {
      int ndim = input->dims().size();
      int rdim = dims.size();
      HANDLE_DIM(4, 3);
      HANDLE_DIM(4, 2);
      HANDLE_DIM(4, 1);
      HANDLE_DIM(3, 2);
      HANDLE_DIMT(3, 1);
      HANDLE_DIM(2, 1);
      HANDLE_DIM(1, 1);
    }
  }

  virtual ~ReduceSumCompute() = default;
};

}  // namespace x86
}  // namespace kernels
}  // namespace lite
}  // namespace paddle
