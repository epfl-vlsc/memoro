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
//#include <tuple>
#include "memoro.h"

namespace memoro {

using NameIDs = std::vector<std::pair<std::string, uint64_t>>;

struct isolatedKeys;

class StackTreeNode {
 public:
  StackTreeNode(uint64_t id, std::string name, const Trace* trace)
      : id_(id), name_(name), trace_(trace) {}

  bool Insert(const Trace*, NameIDs::const_iterator, const NameIDs&);
  double Aggregate(const std::function<double(const Trace* t)>&);
  void Objectify(v8::Isolate*, v8::Local<v8::Object>&, const isolatedKeys&) const;

 private:
  friend class StackTree;
  uint64_t id_;
  std::string name_;  // string_view would be better

  // if trace is not nullptr, there can be no children
  const Trace* trace_ = nullptr;
  std::vector<StackTreeNode> children_;
  double value_ = 0;
};

class StackTree {
 public:
  void SetTraces(std::vector<Trace>&);
  void Aggregate(const std::function<double(const Trace* t)>& f);

  // set args return value to object heirarchy representing tree
  // suitable for the calling JS process
  void V8Objectify(const v8::FunctionCallbackInfo<v8::Value>& args);

  // For other datatype conversions, add an objectify function here
  // and a recursive helper in StackTreeNode

 private:
  bool InsertTrace(const Trace* t);
  void BuildTree();

  // multiple roots are possible because
  // not all traces start in ``main'' for example,
  // some may start in pthread_create() or equivalent
  std::vector<StackTreeNode> roots_;
  std::vector<Trace*> traces_;
  double value_ = 0;  // the sum total of all root aggregate values
};

}  // namespace memoro
