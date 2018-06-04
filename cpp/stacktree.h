
//===-- stacktree.h ------------------------------------------------===//
//
//                     Memoro
//
// This file is distributed under the MIT License.
// See LICENSE for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of Memoro.
// Stuart Byma, EPFL.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <v8.h>
#include <functional>
#include <tuple>
#include "memoro.h"

namespace memoro {

class StackTreeNode;

class StackTree {
 public:
  bool InsertTrace(const Trace* t);
  void Aggregate(std::function<double(const Trace* t)> f);

  // set args return value to object heirarchy representing tree
  // suitable for the calling JS process
  void V8Objectify(const v8::FunctionCallbackInfo<v8::Value>& args);

  // For other datatype conversions, add an objectify function here
  // and a recursive helper in StackTreeNode

 private:
  // multiple roots are possible because
  // not all traces start in ``main'' for example,
  // some may start in pthread_create() or equivalent
  std::vector<StackTreeNode*> roots_;
  double value_ = 0;  // the sum total of all root aggregate values
};

}  // namespace memoro
