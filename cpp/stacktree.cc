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

using NameIDs = vector<pair<string, uint64_t>>;

class StackTreeNode {
 public:
  StackTreeNode(uint64_t id, std::string name, const Trace* trace)
      : id_(id), name_(name), trace_(trace) {}

  bool Insert(const Trace* t, NameIDs::const_iterator pos,
              const NameIDs& name_ids) {
    // auto next = pos+1;
    bool ret = false;
    if (pos == name_ids.end()) {
      // its the last one and will have no children
      // e.g. this is a malloc/new call
      trace_ = t;
      ret = true;
    } else {
      auto it = find_if(children_.begin(), children_.end(),
                        [pos](const StackTreeNode& a) {
                          return a.name_ == pos->first && a.id_ == pos->second;
                        });

      if (it != children_.end()) {
        // exists, advance
        ret = it->Insert(t, pos + 1, name_ids);
      } else {
        // create new
        StackTreeNode n(pos->second, pos->first, nullptr);
        children_.push_back(n);
        ret = children_[children_.size() - 1].Insert(t, pos + 1, name_ids);
      }
    }
    return ret;
  }

  double Aggregate(const std::function<double(const Trace* t)>& f) {
    double ret = 0;
    if (trace_ != nullptr) {
      value_ = f(trace_);
      ret = value_;
    } else {
      //double sum = 0;
      for (auto& child : children_) {
        ret += child.Aggregate(f);
      }
      value_ = ret;
      //return sum;
    }
    return ret;
  }

  void Objectify(Isolate* isolate, Local<Object>& obj) {
    // put myself in this object
    obj->Set(String::NewFromUtf8(isolate, "name"),
             String::NewFromUtf8(isolate, name_.c_str()));
    obj->Set(String::NewFromUtf8(isolate, "value"),
             Number::New(isolate, value_));

    if (trace_ != nullptr) {
      obj->Set(String::NewFromUtf8(isolate, "lifetime_score"),
               Number::New(isolate, trace_->lifetime_score));
      obj->Set(String::NewFromUtf8(isolate, "usage_score"),
               Number::New(isolate, trace_->usage_score));
      obj->Set(String::NewFromUtf8(isolate, "useful_lifetime_score"),
               Number::New(isolate, trace_->useful_lifetime_score));
      return;
    }

    Local<Array> children = Array::New(isolate);

    for (size_t i = 0; i < children_.size(); i++) {
      auto& child = children_[i];
      Local<Object> child_obj = Object::New(isolate);
      child.Objectify(isolate, child_obj);

      children->Set(i, child_obj);
    }

    obj->Set(String::NewFromUtf8(isolate, "children"), children);
  }

 private:
  friend class StackTree;
  uint64_t id_;
  std::string name_;  // string_view would be better

  // if trace is not nullptr, there can be no children
  const Trace* trace_ = nullptr;
  std::vector<StackTreeNode> children_;
  double value_ = 0;
};

bool StackTree::InsertTrace(const Trace* t) {
  // assuming stacktrace of form
  // #1 0x10be26858 in main test.cpp:57

  const string& trace = t->trace;
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

  // now we have a list of function name, ID (address) pairs from
  // `main' to malloc, so to speak ... insert into the stack tree
  auto& first = name_ids[0];
  auto it =
      find_if(roots_.begin(), roots_.end(), [first](const StackTreeNode* a) {
        return a->name_ == first.first && a->id_ == first.second;
      });

  if (it != roots_.end()) {
    // exists, proceed with insert
    return (*it)->Insert(t, name_ids.begin() + 1, name_ids);
  } else {
    // create new root (recall these are entry points, e.g. main,
    // pthread_create, etc. )
    StackTreeNode* n =
        new StackTreeNode(name_ids[0].second, name_ids[0].first, nullptr);
    roots_.push_back(n);
    return roots_[roots_.size() - 1]->Insert(t, name_ids.begin() + 1, name_ids);
  }
}

void StackTree::V8Objectify(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();

  Local<Object> root = Object::New(isolate);
  root->Set(String::NewFromUtf8(isolate, "name"),
            String::NewFromUtf8(isolate, "process"));
  root->Set(String::NewFromUtf8(isolate, "value"),
            Number::New(isolate, value_));

  Local<Array> children = Array::New(isolate);

  for (size_t i = 0; i < roots_.size(); i++) {
    auto& root = roots_[i];
    Local<Object> child_obj = Object::New(isolate);

    root->Objectify(isolate, child_obj);  // recursively

    children->Set(i, child_obj);
  }

  root->Set(String::NewFromUtf8(isolate, "children"), children);

  args.GetReturnValue().Set(root);
}

void StackTree::Aggregate(const std::function<double(const Trace* t)>& f) {
  value_ = 0;
  for (auto& root : roots_) {
    value_ += root->Aggregate(f);
  }
}

}  // namespace memoro
