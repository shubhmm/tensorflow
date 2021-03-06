/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/mlir/tensorflow/utils/convert_tensor.h"

#include <cstring>

#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/StandardTypes.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/compiler/xla/test.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/core/status_test_util.h"
#include "tensorflow/stream_executor/lib/statusor.h"

namespace tensorflow {
namespace {

static void RegisterDialects() {
  static bool init_once = []() {
    mlir::registerDialect<mlir::TF::TensorFlowDialect>();
    return true;
  }();
  (void)init_once;
}

TEST(ConvertTypeToTensorTypeTest, UnrankedTensorType) {
  mlir::MLIRContext context;
  mlir::Builder b(&context);

  PartialTensorShape output_shape =
      ConvertTypeToTensorShape(mlir::UnrankedTensorType::get(b.getF32Type()));
  EXPECT_TRUE(output_shape.IsIdenticalTo(PartialTensorShape()));
}

TEST(ConvertTypeToTensorTypeTest, NonFullyDefinedRankedTensorType) {
  mlir::MLIRContext context;
  mlir::Builder b(&context);

  PartialTensorShape output_shape = ConvertTypeToTensorShape(
      mlir::RankedTensorType::get({-1, 2, 3}, b.getF32Type()));
  EXPECT_TRUE(output_shape.IsIdenticalTo(PartialTensorShape({-1, 2, 3})));
}

TEST(ConvertTypeToTensorTypeTest, FullyDefinedRankedTensorType) {
  mlir::MLIRContext context;
  mlir::Builder b(&context);

  PartialTensorShape output_shape = ConvertTypeToTensorShape(
      mlir::RankedTensorType::get({1, 2, 3}, b.getF32Type()));
  EXPECT_TRUE(output_shape.IsIdenticalTo(PartialTensorShape({1, 2, 3})));
}

TEST(ConvertTypeToTensorTypeTest, ScalarTensorType) {
  mlir::MLIRContext context;
  mlir::Builder b(&context);

  PartialTensorShape output_shape = ConvertTypeToTensorShape(b.getF32Type());
  EXPECT_TRUE(output_shape.IsIdenticalTo(TensorShape()));
}

TEST(ConvertTypeToTensorTypeTest, ConvertStringTensor) {
  RegisterDialects();
  mlir::MLIRContext context;
  mlir::Builder b(&context);

  // Create the sample tensor to convert.
  tensorflow::Tensor tensor(DT_STRING, TensorShape({1, 2, 2, 1}));
  EXPECT_EQ(4, tensor.NumElements());
  auto Tt = tensor.flat<tstring>();
  Tt.setValues({"one", "two", "three", "four"});
  auto value_or_status = ConvertTensor(tensor, &b);
  ASSERT_TRUE(value_or_status.ok());
  auto attr = value_or_status.ValueOrDie();

  EXPECT_TRUE(attr.isa<mlir::DenseStringElementsAttr>());
  auto string_attr = attr.cast<mlir::DenseStringElementsAttr>();
  auto string_values = string_attr.getRawStringData();
  ASSERT_EQ(string_values.size(), 4);
  EXPECT_EQ(string_values[0], mlir::StringRef("one"));
  EXPECT_EQ(string_values[1], mlir::StringRef("two"));
  EXPECT_EQ(string_values[2], mlir::StringRef("three"));
  EXPECT_EQ(string_values[3], mlir::StringRef("four"));
}

}  // namespace
}  // namespace tensorflow
