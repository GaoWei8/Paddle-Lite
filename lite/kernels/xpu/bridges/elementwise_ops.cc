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

#include "lite/kernels/npu/bridges/registry.h"
#include "lite/kernels/xpu/bridges/graph.h"
#include "lite/kernels/xpu/bridges/utility.h"

namespace paddle {
namespace lite {
namespace subgraph {
namespace xpu {

int ElementwiseConverter(void* ctx, OpLite* op, KernelBase* kernel) {
  CHECK(op != nullptr);
  CHECK(ctx != nullptr);
  auto graph = static_cast<Graph*>(ctx);
  auto op_info = op->op_info();
  auto op_type = op_info->Type();
  auto scope = op->scope();
  VLOG(3) << "[XPU] Converting " + op_type + "...";

  // Get input and output vars and op attributes
  auto x_name = op_info->Input("X").front();
  auto x_type = kernel->GetInputDeclType("X");
  CHECK(x_type->precision() == PRECISION(kFloat));
  CHECK(x_type->layout() == DATALAYOUT(kNCHW));
  auto x = scope->FindMutableTensor(x_name);
  auto x_dims = x->dims();
  auto y_name = op_info->Input("Y").front();
  auto y_type = kernel->GetInputDeclType("Y");
  CHECK(y_type->precision() == PRECISION(kFloat));
  CHECK(y_type->layout() == DATALAYOUT(kNCHW));
  auto y = scope->FindMutableTensor(y_name);
  auto y_dims = y->dims();
  auto out_name = op_info->Output("Out").front();
  auto out_type = kernel->GetOutputDeclType("Out");
  CHECK(out_type->precision() == PRECISION(kFloat));
  CHECK(out_type->layout() == DATALAYOUT(kNCHW));
  auto axis = op_info->GetAttr<int>("axis");

  // X node
  std::shared_ptr<xtcl::xExpr> x_node = nullptr;
  if (graph->HasNode(x_name)) {
    x_node = graph->GetNode(x_name);
  } else {
    x_node = graph->AddNode(x_name, x_dims);
  }

  // Y node
  std::shared_ptr<xtcl::xExpr> y_node = nullptr;
  if (graph->HasNode(y_name)) {
    y_node = graph->GetNode(y_name);
  } else {
    y_node = graph->AddNode(y_name, y_dims);
  }

  // Elementwise node
  std::shared_ptr<xtcl::xExpr> elementwise_node = nullptr;
  if (y_dims.size() == 1) {
    elementwise_node = graph->AddNode(
        out_name, graph->builder_.CreateBiasAdd(*x_node, axis, *y_node));
  } else if (x_dims.size() == y_dims.size()) {
    elementwise_node = graph->AddNode(
        out_name, graph->builder_.CreateBinaryOp("add", *x_node, *y_node));
  } else {
    LOG(WARNING)
        << "[XPU] elementwise_add only support y of one dimension, or x "
           "and y of the same dimension. But recieved x's dimension: "
        << x_dims << ", y's dimension: " << y_dims << ", axis: " << axis;
    return FAILED;
  }
  return REBUILD_WHEN_SHAPE_CHANGED;
}

}  // namespace xpu
}  // namespace subgraph
}  // namespace lite
}  // namespace paddle

REGISTER_SUBGRAPH_BRIDGE(XPU,
                         elementwise_add,
                         paddle::lite::subgraph::xpu::ElementwiseConverter);
