
//===-- memoro_node.cc ------------------------------------------------===//
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

#include <node.h>
#include <uv.h>
#include <v8.h>
#include <algorithm>
#include <iostream>
#include "memoro.h"
#include "pattern.h"

using namespace v8;
using namespace memoro;

// primary interface to node/JS here
// technique / org lifted from https://github.com/freezer333/nodecpp-demo
// more or less the interface just converts to/from in memory data structures
// (memoro.h/cc) to V8 JS objects that are passed back to the JS gui layer

struct LoadDatasetWork {
  uv_work_t request;
  Persistent<Function> callback;

  std::string dir_path;
  std::string trace_path;
  std::string chunk_path;
  std::string msg;
  bool result;
};

static void LoadDatasetAsync(uv_work_t* req) {
  LoadDatasetWork* work = static_cast<LoadDatasetWork*>(req->data);

  work->result =
      SetDataset(work->dir_path, work->trace_path, work->chunk_path, work->msg);
}

static void LoadDatasetAsyncComplete(uv_work_t* req, int status) {
  Isolate* isolate = Isolate::GetCurrent();

  // Fix for Node 4.x - thanks to
  // https://github.com/nwjs/blink/commit/ecda32d117aca108c44f38c8eb2cb2d0810dfdeb
  v8::HandleScope handleScope(isolate);

  LoadDatasetWork* work = static_cast<LoadDatasetWork*>(req->data);

  Local<Object> result = Object::New(isolate);
  result->Set(String::NewFromUtf8(isolate, "message"),
              String::NewFromUtf8(isolate, work->msg.c_str()));
  result->Set(String::NewFromUtf8(isolate, "result"),
              Boolean::New(isolate, work->result));

  // set up return arguments
  Local<Value> argv[1] = {result};

  // execute the callback
  Local<Function>::New(isolate, work->callback)
      ->Call(isolate->GetCurrentContext()->Global(), 1, argv);

  // Free up the persistent function callback
  work->callback.Reset();
  delete work;
}

void Memoro_SetDataset(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();

  v8::String::Utf8Value s(args[0]);
  std::string dir_path(*s);

  v8::String::Utf8Value trace_file(args[1]);
  std::string trace_path(*trace_file);

  v8::String::Utf8Value chunk_file(args[2]);
  std::string chunk_path(*chunk_file);

  // launch in a separate thread with callback to keep the gui responsive
  // since this can take some time
  LoadDatasetWork* work = new LoadDatasetWork();
  work->request.data = work;
  work->dir_path = dir_path;
  work->trace_path = trace_path;
  work->chunk_path = chunk_path;

  Local<Function> callback = Local<Function>::Cast(args[3]);
  work->callback.Reset(isolate, callback);

  uv_queue_work(uv_default_loop(), &work->request, LoadDatasetAsync,
                LoadDatasetAsyncComplete);

  args.GetReturnValue().Set(Undefined(isolate));
}

void Memoro_AggregateAll(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  std::vector<TimeValue> values;
  AggregateAll(values);

  auto kTs = String::NewFromUtf8(isolate, "ts");
  auto kValue = String::NewFromUtf8(isolate, "value");

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < values.size(); i++) {
    Local<Object> result = Object::New(isolate);
    result->Set(kTs, Number::New(isolate, values[i].time));
    result->Set(kValue, Number::New(isolate, values[i].value));
    result_list->Set(i, result);
  }

  args.GetReturnValue().Set(result_list);
}

void Memoro_AggregateTrace(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  static std::vector<TimeValue> values;
  values.clear();

  int trace_index = args[0]->NumberValue();

  AggregateTrace(values, trace_index);

  auto kTs = String::NewFromUtf8(isolate, "ts");
  auto kValue = String::NewFromUtf8(isolate, "value");

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < values.size(); i++) {
    Local<Object> result = Object::New(isolate);
    result->Set(kTs, Number::New(isolate, values[i].time));
    result->Set(kValue, Number::New(isolate, values[i].value));
    result_list->Set(i, result);
  }

  args.GetReturnValue().Set(result_list);
}

void Memoro_TraceChunks(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  static std::vector<Chunk*> chunks;
  chunks.clear();

  int trace_index = args[0]->NumberValue();
  int chunk_index = args[1]->NumberValue();
  int num_chunks = args[2]->NumberValue();

  TraceChunks(chunks, trace_index, chunk_index, num_chunks);

  auto kNumReads      = String::NewFromUtf8(isolate, "num_reads");
  auto kNumWrites     = String::NewFromUtf8(isolate, "num_writes");
  auto kSize          = String::NewFromUtf8(isolate, "size");
  auto kTsStart       = String::NewFromUtf8(isolate, "ts_start");
  auto kTsEnd         = String::NewFromUtf8(isolate, "ts_end");
  auto kTsFirst       = String::NewFromUtf8(isolate, "ts_first");
  auto kTsLast        = String::NewFromUtf8(isolate, "ts_last");
  auto kAllocCallTime = String::NewFromUtf8(isolate, "alloc_call_time");
  auto kMultiThread   = String::NewFromUtf8(isolate, "multiThread");
  auto kAccessLow     = String::NewFromUtf8(isolate, "access_interval_low");
  auto kAccessHigh    = String::NewFromUtf8(isolate, "access_interval_high");

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < chunks.size(); i++) {
    Local<Object> result = Object::New(isolate);
    result->Set(kNumReads, Number::New(isolate, chunks[i]->num_reads));
    result->Set(kNumWrites, Number::New(isolate, chunks[i]->num_writes));
    result->Set(kSize, Number::New(isolate, chunks[i]->size));
    result->Set(kTsStart, Number::New(isolate, chunks[i]->timestamp_start));
    result->Set(kTsEnd, Number::New(isolate, chunks[i]->timestamp_end));
    result->Set(kTsFirst, Number::New(isolate, chunks[i]->timestamp_first_access));
    result->Set(kTsLast, Number::New(isolate, chunks[i]->timestamp_last_access));
    result->Set(kAllocCallTime, Number::New(isolate, chunks[i]->alloc_call_time));
    result->Set(kMultiThread, Boolean::New(isolate, chunks[i]->multi_thread));
    result->Set(kAccessLow, Number::New(isolate, chunks[i]->access_interval_low));
    result->Set(kAccessHigh, Number::New(isolate, chunks[i]->access_interval_high));
    result_list->Set(i, result);
  }

  args.GetReturnValue().Set(result_list);
}

void Memoro_Traces(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  static std::vector<TraceValue> traces;  // just reuse this
  traces.clear();
  Traces(traces);

  auto kTrace          = String::NewFromUtf8(isolate, "trace");
  auto kType           = String::NewFromUtf8(isolate, "type");
  auto kTraceIndex     = String::NewFromUtf8(isolate, "trace_index");
  auto kNumChunks      = String::NewFromUtf8(isolate, "num_chunks");
  auto kChunkIndex     = String::NewFromUtf8(isolate, "chunk_index");
  auto kMaxAggregate   = String::NewFromUtf8(isolate, "max_aggregate");
  auto kAllocTimeTotal = String::NewFromUtf8(isolate, "alloc_time_total");
  auto kUsage          = String::NewFromUtf8(isolate, "usage_score");
  auto kLifetime       = String::NewFromUtf8(isolate, "lifetime_score");
  auto kUsefulLifetime = String::NewFromUtf8(isolate, "useful_lifetime_score");

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < traces.size(); i++) {
    Local<Object> result = Object::New(isolate);
    result->Set(kTrace, String::NewFromUtf8(isolate, traces[i].trace->c_str()));
    result->Set(kType, String::NewFromUtf8(isolate, traces[i].type->c_str()));
    result->Set(kTraceIndex, Number::New(isolate, traces[i].trace_index));
    result->Set(kNumChunks, Number::New(isolate, traces[i].num_chunks));
    result->Set(kChunkIndex, Number::New(isolate, traces[i].chunk_index));
    result->Set(kMaxAggregate, Number::New(isolate, traces[i].max_aggregate));
    result->Set(kAllocTimeTotal, Number::New(isolate, traces[i].alloc_time_total));
    result->Set(kUsage, Number::New(isolate, traces[i].usage_score));
    result->Set(kLifetime, Number::New(isolate, traces[i].lifetime_score));
    result->Set(kUsefulLifetime, Number::New(isolate, traces[i].useful_lifetime_score));
    result_list->Set(i, result);
  }

  args.GetReturnValue().Set(result_list);
}

void Memoro_MaxTime(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, MaxTime());

  args.GetReturnValue().Set(retval);
}

void Memoro_MinTime(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, MinTime());

  args.GetReturnValue().Set(retval);
}

void Memoro_FilterMaxTime(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, FilterMaxTime());

  args.GetReturnValue().Set(retval);
}

void Memoro_FilterMinTime(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, FilterMinTime());

  args.GetReturnValue().Set(retval);
}

void Memoro_MaxAggregate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, MaxAggregate());

  args.GetReturnValue().Set(retval);
}

void Memoro_GlobalAllocTime(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, GlobalAllocTime());

  args.GetReturnValue().Set(retval);
}

void Memoro_SetTraceKeyword(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::String::Utf8Value s(args[0]);
  std::string keyword(*s);

  SetTraceKeyword(keyword);
}

void Memoro_SetTypeKeyword(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::String::Utf8Value s(args[0]);
  std::string keyword(*s);

  SetTypeKeyword(keyword);
}

void Memoro_SetFilterMinMax(const v8::FunctionCallbackInfo<v8::Value>& args) {
  uint64_t t1 = args[0]->NumberValue();
  uint64_t t2 = args[1]->NumberValue();
  SetFilterMinMax(t1, t2);
}

void Memoro_TraceFilterReset(const v8::FunctionCallbackInfo<v8::Value>& args) {
  TraceFilterReset();
}

void Memoro_TypeFilterReset(const v8::FunctionCallbackInfo<v8::Value>& args) {
  TypeFilterReset();
}

void Memoro_FilterMinMaxReset(const v8::FunctionCallbackInfo<v8::Value>& args) {
  FilterMinMaxReset();
}

void Memoro_Inefficiencies(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();

  int trace_index = args[0]->NumberValue();
  uint64_t i = Inefficiencies(trace_index);

  // there will be more here eventually, probably
  Local<Object> result = Object::New(isolate);
  result->Set(String::NewFromUtf8(isolate, "unused"),
              Boolean::New(isolate, HasInefficiency(i, Inefficiency::Unused)));
  result->Set(
      String::NewFromUtf8(isolate, "write_only"),
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::WriteOnly)));
  result->Set(
      String::NewFromUtf8(isolate, "read_only"),
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::ReadOnly)));
  result->Set(
      String::NewFromUtf8(isolate, "short_lifetime"),
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::ShortLifetime)));
  result->Set(
      String::NewFromUtf8(isolate, "late_free"),
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::LateFree)));
  result->Set(
      String::NewFromUtf8(isolate, "early_alloc"),
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::EarlyAlloc)));
  result->Set(String::NewFromUtf8(isolate, "increasing_allocs"),
              Boolean::New(isolate, HasInefficiency(
                                        i, Inefficiency::IncreasingReallocs)));
  result->Set(String::NewFromUtf8(isolate, "top_percentile_size"),
              Boolean::New(isolate, HasInefficiency(
                                        i, Inefficiency::TopPercentileSize)));
  result->Set(String::NewFromUtf8(isolate, "top_percentile_chunks"),
              Boolean::New(isolate, HasInefficiency(
                                        i, Inefficiency::TopPercentileChunks)));
  result->Set(
      String::NewFromUtf8(isolate, "multi_thread"),
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::MultiThread)));
  result->Set(String::NewFromUtf8(isolate, "low_access_coverage"),
              Boolean::New(isolate, HasInefficiency(
                                        i, Inefficiency::LowAccessCoverage)));

  args.GetReturnValue().Set(result);
}

void Memoro_StackTree(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // was hoping to keep all the V8 stuff in this file, oh well ..
  StackTreeObject(args);
}

void Memoro_StackTreeByBytes(const v8::FunctionCallbackInfo<v8::Value>& args) {
  uint64_t time = args[0]->NumberValue();

  StackTreeAggregate([time](const Trace* t) -> double {
    auto it = find_if(t->aggregate.begin(), t->aggregate.end(),
                      [time](const TimeValue& a) { return a.time >= time; });
    if (it == t->aggregate.end()) {
      // std::cout << "found no time";
      return (double)0;
    }
    // take the prev value when our point was actually greater than the selected
    // time
    if (it != t->aggregate.begin() && it->time > time) it--;
    // std::cout << "value is " << it->value << " at time " << it->time << " for
    // trace " << t->trace << std::endl;
    return (double)it->value;
  });
}

void Memoro_StackTreeByNumAllocs(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  StackTreeAggregate(
      [](const Trace* t) -> double { return (double)t->chunks.size(); });
}

void init(Handle<Object> exports, Handle<Object> module) {
  NODE_SET_METHOD(exports, "set_dataset", Memoro_SetDataset);
  NODE_SET_METHOD(exports, "aggregate_all", Memoro_AggregateAll);
  NODE_SET_METHOD(exports, "max_time", Memoro_MaxTime);
  NODE_SET_METHOD(exports, "min_time", Memoro_MinTime);
  NODE_SET_METHOD(exports, "filter_max_time", Memoro_FilterMaxTime);
  NODE_SET_METHOD(exports, "filter_min_time", Memoro_FilterMinTime);
  NODE_SET_METHOD(exports, "max_aggregate", Memoro_MaxAggregate);
  NODE_SET_METHOD(exports, "set_trace_keyword", Memoro_SetTraceKeyword);
  NODE_SET_METHOD(exports, "set_type_keyword", Memoro_SetTypeKeyword);
  NODE_SET_METHOD(exports, "traces", Memoro_Traces);
  NODE_SET_METHOD(exports, "aggregate_trace", Memoro_AggregateTrace);
  NODE_SET_METHOD(exports, "trace_chunks", Memoro_TraceChunks);
  NODE_SET_METHOD(exports, "set_filter_minmax", Memoro_SetFilterMinMax);
  NODE_SET_METHOD(exports, "trace_filter_reset", Memoro_TraceFilterReset);
  NODE_SET_METHOD(exports, "type_filter_reset", Memoro_TypeFilterReset);
  NODE_SET_METHOD(exports, "filter_minmax_reset", Memoro_FilterMinMaxReset);
  NODE_SET_METHOD(exports, "inefficiencies", Memoro_Inefficiencies);
  NODE_SET_METHOD(exports, "global_alloc_time", Memoro_GlobalAllocTime);
  NODE_SET_METHOD(exports, "stacktree", Memoro_StackTree);
  NODE_SET_METHOD(exports, "stacktree_by_bytes", Memoro_StackTreeByBytes);
  NODE_SET_METHOD(exports, "stacktree_by_numallocs",
                  Memoro_StackTreeByNumAllocs);
}

NODE_MODULE(memoro, init)
