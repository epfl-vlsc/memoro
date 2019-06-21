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

#include "stacktree.h"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace memoro {

using namespace std;
using namespace v8;

struct isolatedKeys {
  Local<String> kName, kProcess, kValue;
  Local<String> kChildren;
  Local<String> kLifetime, kUsage, kUsefulLifetime;
};

bool StackTreeNode::Insert(const TraceAndValue& tv, NameIDs::const_iterator pos,
    const NameIDs& name_ids) {
  // auto next = pos+1;
  bool ret = false;
  if (pos == name_ids.end()) {
    // its the last one and will have no children
    // e.g. this is a malloc/new call
    trace_ = tv.trace;
    value_ = tv.value;
    ret = true;
  } else {
    auto it = find_if(children_.begin(), children_.end(),
        [pos](const StackTreeNode& a) {
        return a.name_ == pos->first && a.id_ == pos->second;
        });

    if (it != children_.end()) {
      // exists, advance
      ret = it->Insert(tv, pos + 1, name_ids);
    } else {
      // create new
      children_.emplace_back(pos->second, pos->first, nullptr);
      ret = children_.back().Insert(tv, pos + 1, name_ids);
    }
  }
  return ret;
}

double StackTreeNode::Aggregate() {
  if (trace_ != nullptr)
    return value_;

  double ret = 0;
  for (auto& child : children_)
    ret += child.Aggregate();

  return value_ = ret;
}

void StackTreeNode::Objectify(Isolate* isolate, Local<Object>& obj, const isolatedKeys& keys) const {
  // put myself in this object
  obj->Set(keys.kName,
      String::NewFromUtf8(isolate, name_.c_str()));
  obj->Set(keys.kValue,
      Number::New(isolate, value_));

  if (trace_ != nullptr) {
    obj->Set(keys.kLifetime,
        Number::New(isolate, trace_->lifetime_score));
    obj->Set(keys.kUsage,
        Number::New(isolate, trace_->usage_score));
    obj->Set(keys.kUsefulLifetime,
        Number::New(isolate, trace_->useful_lifetime_score));
    return;
  }

  Local<Array> children = Array::New(isolate);

  for (size_t i = 0; i < children_.size(); i++) {
    auto& child = children_[i];
    Local<Object> child_obj = Object::New(isolate);
    child.Objectify(isolate, child_obj, keys);

    children->Set(i, child_obj);
  }

  obj->Set(keys.kChildren, children);
}

bool StackTreeNodeHide::Insert(const TraceAndValue& tv, NameIDs::const_iterator pos, const NameIDs& nameIds) {
  if (children_.size() < MAX_TRACES) {
    return StackTreeNode::Insert(tv, pos, nameIds);
  } else {
    if (next_ == nullptr)
      next_ = std::make_unique<StackTreeNodeHide>();

    return next_->Insert(tv, pos, nameIds);
  }
}

double StackTreeNodeHide::Aggregate() {
  value_ = StackTreeNode::Aggregate();

  if (next_)
    value_ += next_->Aggregate();

  return value_;
}

void StackTreeNodeHide::Objectify(Isolate* isolate, Local<Object>& obj, const isolatedKeys& keys) const {
  // put myself in this object
  obj->Set(keys.kName,
      String::NewFromUtf8(isolate, name_.c_str()));
  obj->Set(keys.kValue,
      Number::New(isolate, value_));

  if (next_) {
    Local<Array> children = Array::New(isolate);

    Local<Object> next_obj = Object::New(isolate);
    next_->Objectify(isolate, next_obj, keys);
    children->Set(0, next_obj);

    obj->Set(keys.kChildren, children);
  }
}

bool StackTree::InsertTrace(const TraceAndValue& tv) {
  // assuming stacktrace of form
  // #1 0x10be26858 in main test.cpp:57

  const string& trace = tv.trace->trace;
  NameIDs name_ids;
  stringstream ss;
  string name;
  uint64_t addr = 0;

  auto position = find(trace.rbegin(), trace.rend(), '#');

  // this is done once, so we are fine with all this
  // C++-y nonsense that allocates stuff :-P
  // #27 0x118b1a035  (<unknown module>)|
  while (position != trace.rend()) {
    ss.clear();
    size_t pos = trace.rend() - position;  // backward :-P
    size_t p1 = trace.find_first_of(' ', pos);
    size_t p2 = trace.find_first_of(' ', p1 + 1);
    ss << std::hex << trace.substr(p1 + 1, p2 - p1);
    ss >> addr;
    if (trace[p2 + 2] == '(')
      name = "unknown";  // unknown module
    else {
      p1 = trace.find_first_of(' ', p2 + 1);
      p2 = trace.find_first_of('|', p1 + 1);
      name = trace.substr(p1 + 1, p2 - p1 - 1);
    }

    name_ids.push_back(make_pair(name, addr));

    position = find(position + 1, trace.rend(), '#');
  }

  // Cap the number of node at MAX_TRACES and hide the rest
  if (node_count_++ >= MAX_TRACES) {
    if (hide_ == nullptr)
      hide_ = std::make_unique<StackTreeNodeHide>();

    return hide_->Insert(tv, name_ids.begin() + 1, name_ids);
  }

  // now we have a list of function name, ID (address) pairs from
  // `main' to malloc, so to speak ... insert into the stack tree
  auto& first = name_ids[0];
  auto it =
      find_if(roots_.begin(), roots_.end(), [first](const StackTreeNode& a) {
        return a.name_ == first.first && a.id_ == first.second;
      });

  if (it != roots_.end()) {
    // exists, proceed with insert
    return (*it).Insert(tv, name_ids.begin() + 1, name_ids);
  } else {
    // create new root (recall these are entry points, e.g. main,
    // pthread_create, etc. )
    roots_.emplace_back(name_ids[0].second, name_ids[0].first, nullptr);
    return roots_.back().Insert(tv, name_ids.begin() + 1, name_ids);
  }
}

void StackTree::BuildTree() {
  node_count_ = 0;
  roots_.clear();
  hide_.reset(nullptr);
  for (auto it = traces_.cbegin(); it != traces_.cend(); it++) {
    (*it).trace->filtered = node_count_ >= MAX_TRACES;
    InsertTrace(*it);
  }
}

void StackTree::SetTraces(std::vector<Trace>& traces) {
  traces_.clear();
  traces_.reserve(traces.size());
  for (Trace& t: traces)
    traces_.push_back({ &t, 0.0 });
}

void StackTree::V8Objectify(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  const isolatedKeys keys = {
    String::NewFromUtf8(isolate, "name"),
    String::NewFromUtf8(isolate, "process"),
    String::NewFromUtf8(isolate, "value"),
    String::NewFromUtf8(isolate, "children"),

    String::NewFromUtf8(isolate, "lifetime_score"),
    String::NewFromUtf8(isolate, "usage_score"),
    String::NewFromUtf8(isolate, "useful_lifetime_score"),
  };

  Local<Object> root = Object::New(isolate);
  root->Set(keys.kName, keys.kProcess);
  root->Set(keys.kValue, Number::New(isolate, value_));

  Local<Array> children = Array::New(isolate);

  for (size_t i = 0; i < roots_.size(); i++) {
    const auto& root = roots_[i];
    Local<Object> child_obj = Object::New(isolate);

    root.Objectify(isolate, child_obj, keys);  // recursively

    children->Set(i, child_obj);
  }

  if (hide_) {
    Local<Object> hide_obj = Object::New(isolate);

    hide_->Objectify(isolate, hide_obj, keys);  // recursively

    children->Set(roots_.size(), hide_obj);
  }

  root->Set(keys.kChildren, children);

  args.GetReturnValue().Set(root);
}

void StackTree::Aggregate(const std::function<double(const Trace* t)>& f) {
  for (auto& tv: traces_)
    tv.value = f(tv.trace);

  sort(traces_.begin(), traces_.end(),
      [](const TraceAndValue& a, const TraceAndValue& b) {
        return a.value > b.value; });

  BuildTree();

  value_ = 0;
  for (auto& root : roots_) {
    value_ += root.Aggregate();
  }

  if (hide_)
    value_ += hide_->Aggregate();
}

}  // namespace memoro
