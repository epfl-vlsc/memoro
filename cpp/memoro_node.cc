
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
#include <numeric>
#include <iostream>
#include <queue>
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

struct LoadDatasetStatsWork {
  uv_work_t request;
  Persistent<Function> callback;

  std::string dir_path;
  std::string stats_path;
  std::string msg;
  bool result;
};

static void LoadDatasetAsync(uv_work_t* req) {
  LoadDatasetWork* work = static_cast<LoadDatasetWork*>(req->data);

  work->result =
      SetDataset(work->dir_path, work->trace_path, work->chunk_path, work->msg);
}

static void LoadDatasetStatsAsync(uv_work_t* req) {
  LoadDatasetStatsWork* work = static_cast<LoadDatasetStatsWork*>(req->data);

  work->result =
      SetDataset(work->dir_path, work->stats_path, work->msg);
}

static void LoadDatasetAsyncComplete(uv_work_t* req, int status) {
  Isolate* isolate = Isolate::GetCurrent();
  Local<Context> context = isolate->GetCurrentContext();

  // Fix for Node 4.x - thanks to
  // https://github.com/nwjs/blink/commit/ecda32d117aca108c44f38c8eb2cb2d0810dfdeb
  v8::HandleScope handleScope(isolate);

  LoadDatasetWork* work = static_cast<LoadDatasetWork*>(req->data);

  Local<Object> result = Object::New(isolate);
  result->Set(context, String::NewFromUtf8(isolate, "message", NewStringType::kInternalized).ToLocalChecked(),
              String::NewFromUtf8(isolate, work->msg.c_str(), NewStringType::kInternalized).ToLocalChecked()).Check();
  result->Set(context, String::NewFromUtf8(isolate, "result", NewStringType::kInternalized).ToLocalChecked(),
              Boolean::New(isolate, work->result)).Check();

  // set up return arguments
  Local<Value> argv[1] = {result};

  // execute the callback
  Local<Function>::New(isolate, work->callback)
      ->Call(context, context->Global(), 1, argv).ToLocalChecked();

  // Free up the persistent function callback
  work->callback.Reset();
  delete work;
}

static void LoadDatasetStatsAsyncComplete(uv_work_t* req, int status) {
  Isolate* isolate = Isolate::GetCurrent();
  Local<Context> context = isolate->GetCurrentContext();

  // Fix for Node 4.x - thanks to
  // https://github.com/nwjs/blink/commit/ecda32d117aca108c44f38c8eb2cb2d0810dfdeb
  v8::HandleScope handleScope(isolate);

  LoadDatasetStatsWork* work = static_cast<LoadDatasetStatsWork*>(req->data);

  Local<Object> result = Object::New(isolate);
  result->Set(context, String::NewFromUtf8(isolate, "message", NewStringType::kInternalized).ToLocalChecked(),
              String::NewFromUtf8(isolate, work->msg.c_str(), NewStringType::kInternalized).ToLocalChecked()).Check();
  result->Set(context, String::NewFromUtf8(isolate, "result", NewStringType::kInternalized).ToLocalChecked(),
              Boolean::New(isolate, work->result)).Check();

  // set up return arguments
  Local<Value> argv[1] = {result};

  // execute the callback
  Local<Function>::New(isolate, work->callback)
      ->Call(context, context->Global(), 1, argv).ToLocalChecked();

  // Free up the persistent function callback
  work->callback.Reset();
  delete work;
}

void Memoro_SetDataset(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();

  v8::String::Utf8Value s(isolate, args[0]);
  std::string dir_path(*s);

  v8::String::Utf8Value trace_file(isolate, args[1]);
  std::string trace_path(*trace_file);

  v8::String::Utf8Value chunk_file(isolate, args[2]);
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

void Memoro_SetDatasetStats(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();

  v8::String::Utf8Value s(isolate, args[0]);
  std::string dir_path(*s);

  v8::String::Utf8Value stats_file(isolate, args[1]);
  std::string stats_path(*stats_file);

  // launch in a separate thread with callback to keep the gui responsive
  // since this can take some time
  LoadDatasetStatsWork* work = new LoadDatasetStatsWork();
  work->request.data = work;
  work->dir_path = dir_path;
  work->stats_path = stats_path;

  Local<Function> callback = Local<Function>::Cast(args[2]);
  work->callback.Reset(isolate, callback);

  uv_queue_work(uv_default_loop(), &work->request, LoadDatasetStatsAsync,
                LoadDatasetStatsAsyncComplete);

  args.GetReturnValue().Set(Undefined(isolate));
}

void Memoro_AggregateAll(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  std::vector<TimeValue> values;
  AggregateAll(values);

  auto kTs = String::NewFromUtf8(isolate, "ts", NewStringType::kInternalized).ToLocalChecked();
  auto kValue = String::NewFromUtf8(isolate, "value", NewStringType::kInternalized).ToLocalChecked();

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < values.size(); i++) {
    Local<Object> result = Object::New(isolate);
    result->Set(context, kTs, Number::New(isolate, values[i].time)).Check();
    result->Set(context, kValue, Number::New(isolate, values[i].value)).Check();
    result_list->Set(context, i, result).Check();
  }

  args.GetReturnValue().Set(result_list);
}

void Memoro_AggregateTrace(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  static std::vector<TimeValue> values;
  values.clear();

  int trace_index = args[0]->NumberValue(context).ToChecked();

  AggregateTrace(values, trace_index);

  auto kTs = String::NewFromUtf8(isolate, "ts", NewStringType::kInternalized).ToLocalChecked();
  auto kValue = String::NewFromUtf8(isolate, "value", NewStringType::kInternalized).ToLocalChecked();

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < values.size(); i++) {
    Local<Object> result = Object::New(isolate);
    result->Set(context, kTs, Number::New(isolate, values[i].time)).Check();
    result->Set(context, kValue, Number::New(isolate, values[i].value)).Check();
    result_list->Set(context, i, result).Check();
  }

  args.GetReturnValue().Set(result_list);
}

void Memoro_TraceChunks(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  static std::vector<Chunk*> chunks;
  chunks.clear();

  int trace_index = args[0]->NumberValue(context).ToChecked();
  int chunk_index = args[1]->NumberValue(context).ToChecked();
  int num_chunks = args[2]->NumberValue(context).ToChecked();

  TraceChunks(chunks, trace_index, chunk_index, num_chunks);

  auto kNumReads      = String::NewFromUtf8(isolate, "num_reads", NewStringType::kInternalized).ToLocalChecked();
  auto kNumWrites     = String::NewFromUtf8(isolate, "num_writes", NewStringType::kInternalized).ToLocalChecked();
  auto kSize          = String::NewFromUtf8(isolate, "size", NewStringType::kInternalized).ToLocalChecked();
  auto kTsStart       = String::NewFromUtf8(isolate, "ts_start", NewStringType::kInternalized).ToLocalChecked();
  auto kTsEnd         = String::NewFromUtf8(isolate, "ts_end", NewStringType::kInternalized).ToLocalChecked();
  auto kTsFirst       = String::NewFromUtf8(isolate, "ts_first", NewStringType::kInternalized).ToLocalChecked();
  auto kTsLast        = String::NewFromUtf8(isolate, "ts_last", NewStringType::kInternalized).ToLocalChecked();
  auto kAllocCallTime = String::NewFromUtf8(isolate, "alloc_call_time", NewStringType::kInternalized).ToLocalChecked();
  auto kMultiThread   = String::NewFromUtf8(isolate, "multiThread", NewStringType::kInternalized).ToLocalChecked();
  auto kAccessLow     = String::NewFromUtf8(isolate, "access_interval_low", NewStringType::kInternalized).ToLocalChecked();
  auto kAccessHigh    = String::NewFromUtf8(isolate, "access_interval_high", NewStringType::kInternalized).ToLocalChecked();

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < chunks.size(); i++) {
    Local<Object> result = Object::New(isolate);
    result->Set(context, kNumReads, Number::New(isolate, chunks[i]->num_reads)).Check();
    result->Set(context, kNumWrites, Number::New(isolate, chunks[i]->num_writes)).Check();
    result->Set(context, kSize, Number::New(isolate, chunks[i]->size)).Check();
    result->Set(context, kTsStart, Number::New(isolate, chunks[i]->timestamp_start)).Check();
    result->Set(context, kTsEnd, Number::New(isolate, chunks[i]->timestamp_end)).Check();
    result->Set(context, kTsFirst, Number::New(isolate, chunks[i]->timestamp_first_access)).Check();
    result->Set(context, kTsLast, Number::New(isolate, chunks[i]->timestamp_last_access)).Check();
    result->Set(context, kAllocCallTime, Number::New(isolate, chunks[i]->alloc_call_time)).Check();
    result->Set(context, kMultiThread, Boolean::New(isolate, chunks[i]->multi_thread)).Check();
    result->Set(context, kAccessLow, Number::New(isolate, chunks[i]->access_interval_low)).Check();
    result->Set(context, kAccessHigh, Number::New(isolate, chunks[i]->access_interval_high)).Check();
    result_list->Set(context, i, result).Check();
  }

  args.GetReturnValue().Set(result_list);
}

static std::vector<TraceValue> traces;  // just reuse this
void Memoro_SortTraces(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  printf("Collecting traces\n");
  traces.clear();
  Traces(traces);

  v8::String::Utf8Value sortByV8(isolate, args[0]);
  std::string sortBy(*sortByV8, sortByV8.length());

  if (sortBy == "bytes") {
    sort(traces.begin(), traces.end(), [](const TraceValue& a, const TraceValue&b) {
        return b.max_aggregate < a.max_aggregate; });
  } else if (sortBy == "allocations") {
    sort(traces.begin(), traces.end(), [](const TraceValue& a, const TraceValue&b) {
        return b.num_chunks < a.num_chunks; });
  } else if (sortBy == "usage") {
    sort(traces.begin(), traces.end(), [](const TraceValue& a, const TraceValue&b) {
        return b.usage_score < a.usage_score; });
  } else if (sortBy == "lifetime") {
    sort(traces.begin(), traces.end(), [](const TraceValue& a, const TraceValue&b) {
        return b.lifetime_score < a.lifetime_score; });
  } else if (sortBy == "useful_lifetime") {
    sort(traces.begin(), traces.end(), [](const TraceValue& a, const TraceValue&b) {
        return b.useful_lifetime_score < a.useful_lifetime_score; });
  }
}

void Memoro_Traces(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();

  size_t offset = args[0]->IntegerValue(context).ToChecked();
  size_t limit = args[1]->IntegerValue(context).ToChecked();
  size_t count = std::min(limit, traces.size() - offset);
  if (offset == 0 && limit == 0) count = traces.size();

  auto kTrace          = String::NewFromUtf8(isolate, "trace", NewStringType::kInternalized).ToLocalChecked();
  auto kType           = String::NewFromUtf8(isolate, "type", NewStringType::kInternalized).ToLocalChecked();
  auto kTraceIndex     = String::NewFromUtf8(isolate, "trace_index", NewStringType::kInternalized).ToLocalChecked();
  auto kNumChunks      = String::NewFromUtf8(isolate, "num_chunks", NewStringType::kInternalized).ToLocalChecked();
  auto kChunkIndex     = String::NewFromUtf8(isolate, "chunk_index", NewStringType::kInternalized).ToLocalChecked();
  auto kMaxAggregate   = String::NewFromUtf8(isolate, "max_aggregate", NewStringType::kInternalized).ToLocalChecked();
  auto kAllocTimeTotal = String::NewFromUtf8(isolate, "alloc_time_total", NewStringType::kInternalized).ToLocalChecked();
  auto kUsage          = String::NewFromUtf8(isolate, "usage_score", NewStringType::kInternalized).ToLocalChecked();
  auto kLifetime       = String::NewFromUtf8(isolate, "lifetime_score", NewStringType::kInternalized).ToLocalChecked();
  auto kUsefulLifetime = String::NewFromUtf8(isolate, "useful_lifetime_score", NewStringType::kInternalized).ToLocalChecked();

  Local<Array> result_list = Array::New(isolate);
  for (size_t i = 0; i < count; i++) {
    const TraceValue& t = traces[i + offset];

    Local<Object> result = Object::New(isolate);
    result->Set(context, kTrace, String::NewFromUtf8(isolate, t.trace.data(), NewStringType::kInternalized).ToLocalChecked()).Check();
    result->Set(context, kType, String::NewFromUtf8(isolate, t.type.data(), NewStringType::kInternalized).ToLocalChecked()).Check();
    result->Set(context, kTraceIndex, Number::New(isolate, t.trace_index)).Check();
    result->Set(context, kNumChunks, Number::New(isolate, t.num_chunks)).Check();
    result->Set(context, kChunkIndex, Number::New(isolate, t.chunk_index)).Check();
    result->Set(context, kMaxAggregate, Number::New(isolate, t.max_aggregate)).Check();
    result->Set(context, kAllocTimeTotal, Number::New(isolate, t.alloc_time_total)).Check();
    result->Set(context, kUsage, Number::New(isolate, t.usage_score)).Check();
    result->Set(context, kLifetime, Number::New(isolate, t.lifetime_score)).Check();
    result->Set(context, kUsefulLifetime, Number::New(isolate, t.useful_lifetime_score)).Check();

    result_list->Set(context, i, result).Check();
  }

  args.GetReturnValue().Set(result_list);
}

void Memoro_GlobalInfo(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  GlobalInfo info;
  GlobalInformation(info);

  auto kNumTraces         = String::NewFromUtf8(isolate, "num_traces", NewStringType::kInternalized).ToLocalChecked();
  auto kNumChunks         = String::NewFromUtf8(isolate, "num_chunks", NewStringType::kInternalized).ToLocalChecked();
  auto kAggMax            = String::NewFromUtf8(isolate, "aggregate_max", NewStringType::kInternalized).ToLocalChecked();
  auto kTotalTime         = String::NewFromUtf8(isolate, "total_time", NewStringType::kInternalized).ToLocalChecked();
  auto kAllocTime         = String::NewFromUtf8(isolate, "alloc_time", NewStringType::kInternalized).ToLocalChecked();
  auto kUsageAvg          = String::NewFromUtf8(isolate, "usage_avg", NewStringType::kInternalized).ToLocalChecked();
  auto kUsageVar          = String::NewFromUtf8(isolate, "usage_var", NewStringType::kInternalized).ToLocalChecked();
  auto kLifetimeAvg       = String::NewFromUtf8(isolate, "lifetime_avg", NewStringType::kInternalized).ToLocalChecked();
  auto kLifetimeVar       = String::NewFromUtf8(isolate, "lifetime_var", NewStringType::kInternalized).ToLocalChecked();
  auto kUsefulLifetimeAvg = String::NewFromUtf8(isolate, "useful_lifetime_avg", NewStringType::kInternalized).ToLocalChecked();
  auto kUsefulLifetimeVar = String::NewFromUtf8(isolate, "useful_lifetime_var", NewStringType::kInternalized).ToLocalChecked();

  Local<Object> result = Object::New(isolate);
  result->Set(context, kNumTraces, Number::New(isolate, info.num_traces)).Check();
  result->Set(context, kNumChunks, Number::New(isolate, info.num_chunks)).Check();
  result->Set(context, kAggMax, Number::New(isolate, info.aggregate_max)).Check();
  result->Set(context, kTotalTime, Number::New(isolate, info.total_time)).Check();
  result->Set(context, kAllocTime, Number::New(isolate, info.alloc_time)).Check();
  result->Set(context, kUsageAvg, Number::New(isolate, info.usage_avg)).Check();
  result->Set(context, kUsageVar, Number::New(isolate, info.usage_var)).Check();
  result->Set(context, kLifetimeAvg, Number::New(isolate, info.lifetime_avg)).Check();
  result->Set(context, kLifetimeVar, Number::New(isolate, info.lifetime_var)).Check();
  result->Set(context, kUsefulLifetimeAvg, Number::New(isolate, info.useful_lifetime_avg)).Check();
  result->Set(context, kUsefulLifetimeVar, Number::New(isolate, info.useful_lifetime_var)).Check();

  args.GetReturnValue().Set(result);
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
  Isolate* isolate = args.GetIsolate();
  v8::String::Utf8Value s(isolate, args[0]);
  std::string keyword(*s);

  SetTraceKeyword(keyword);
}

void Memoro_SetTypeKeyword(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  v8::String::Utf8Value s(isolate, args[0]);
  std::string keyword(*s);

  SetTypeKeyword(keyword);
}

void Memoro_SetFilterMinMax(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  uint64_t t1 = args[0]->NumberValue(context).ToChecked();
  uint64_t t2 = args[1]->NumberValue(context).ToChecked();
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
  Local<Context> context = isolate->GetCurrentContext();

  int trace_index = args[0]->NumberValue(context).ToChecked();
  uint64_t i = Inefficiencies(trace_index);

  // there will be more here eventually, probably
  Local<Object> result = Object::New(isolate);
  result->Set(context, String::NewFromUtf8(isolate, "unused", NewStringType::kInternalized).ToLocalChecked(),
              Boolean::New(isolate, HasInefficiency(i, Inefficiency::Unused))).Check();
  result->Set(context,
      String::NewFromUtf8(isolate, "write_only", NewStringType::kInternalized).ToLocalChecked(),
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::WriteOnly))).Check();
  result->Set(context,
      String::NewFromUtf8(isolate, "read_only", NewStringType::kInternalized).ToLocalChecked(),
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::ReadOnly))).Check();
  result->Set(context,
      String::NewFromUtf8(isolate, "short_lifetime", NewStringType::kInternalized).ToLocalChecked(),
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::ShortLifetime))).Check();
  result->Set(context,
      String::NewFromUtf8(isolate, "late_free", NewStringType::kInternalized).ToLocalChecked(),
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::LateFree))).Check();
  result->Set(context,
      String::NewFromUtf8(isolate, "early_alloc", NewStringType::kInternalized).ToLocalChecked(),
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::EarlyAlloc))).Check();
  result->Set(context, String::NewFromUtf8(isolate, "increasing_allocs", NewStringType::kInternalized).ToLocalChecked(),
              Boolean::New(isolate, HasInefficiency(
                                        i, Inefficiency::IncreasingReallocs))).Check();
  result->Set(context, String::NewFromUtf8(isolate, "top_percentile_size", NewStringType::kInternalized).ToLocalChecked(),
              Boolean::New(isolate, HasInefficiency(
                                        i, Inefficiency::TopPercentileSize))).Check();
  result->Set(context, String::NewFromUtf8(isolate, "top_percentile_chunks", NewStringType::kInternalized).ToLocalChecked(),
              Boolean::New(isolate, HasInefficiency(
                                        i, Inefficiency::TopPercentileChunks))).Check();
  result->Set(context,
      String::NewFromUtf8(isolate, "multi_thread", NewStringType::kInternalized).ToLocalChecked(),
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::MultiThread))).Check();
  result->Set(context, String::NewFromUtf8(isolate, "low_access_coverage", NewStringType::kInternalized).ToLocalChecked(),
              Boolean::New(isolate, HasInefficiency(
                                        i, Inefficiency::LowAccessCoverage))).Check();

  args.GetReturnValue().Set(result);
}

void Memoro_StackTree(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // was hoping to keep all the V8 stuff in this file, oh well ..
  StackTreeObject(args);
}

void Memoro_StackTreeByBytes(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  uint64_t time = args[0]->NumberValue(context).ToChecked();

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
  }, "ByBytesInTime");
}

void Memoro_StackTreeByBytesTotal(const v8::FunctionCallbackInfo<v8::Value>& args) {
  StackTreeAggregate(
      [](const Trace* t) -> double {
        return accumulate(t->chunks.begin(), t->chunks.end(), 0.0,
          [](double sum, const Chunk* c) { return sum + c->size; });
      }, "ByBytesTotal");
}

void Memoro_StackTreeByNumAllocs(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  StackTreeAggregate(
      [](const Trace* t) -> double { return (double)t->chunks.size(); },
      "ByNumAllocs");
}

void Memoro_StackTreeByPeakWaste(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  StackTreeAggregate(
      [](const Trace* t) -> double {
        int64_t peak_waste = 0, acc = 0;
        std::priority_queue<TimeValue> Q;

        uint64_t i = 0;
        while (i < t->chunks.size()) {
          const auto* C = t->chunks[i];

          if (!Q.empty() && Q.top().time < C->timestamp_start) {
            acc -= Q.top().value;
            Q.pop();
          } else {
            int64_t total_bytes = C->size;
            int64_t used_bytes = C->access_interval_high - C->access_interval_low;
            int64_t wasted_bytes = total_bytes - used_bytes;

            acc += wasted_bytes;
            peak_waste = std::max(acc, peak_waste);

            Q.push({ C->timestamp_end, wasted_bytes });
            ++i;
          }
        }

        return peak_waste;
      },
      "ByPeakWaste");
}

void Memoro_TraceLimit(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  uint64_t limit = args[0]->NumberValue(context).ToChecked();
  TraceLimit(limit);
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "set_dataset", Memoro_SetDataset);
  NODE_SET_METHOD(exports, "set_dataset_stats", Memoro_SetDatasetStats);
  NODE_SET_METHOD(exports, "aggregate_all", Memoro_AggregateAll);
  NODE_SET_METHOD(exports, "global_info", Memoro_GlobalInfo);
  NODE_SET_METHOD(exports, "max_time", Memoro_MaxTime);
  NODE_SET_METHOD(exports, "min_time", Memoro_MinTime);
  NODE_SET_METHOD(exports, "filter_max_time", Memoro_FilterMaxTime);
  NODE_SET_METHOD(exports, "filter_min_time", Memoro_FilterMinTime);
  NODE_SET_METHOD(exports, "max_aggregate", Memoro_MaxAggregate);
  NODE_SET_METHOD(exports, "set_trace_keyword", Memoro_SetTraceKeyword);
  NODE_SET_METHOD(exports, "set_type_keyword", Memoro_SetTypeKeyword);
  NODE_SET_METHOD(exports, "sort_traces", Memoro_SortTraces);
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
  NODE_SET_METHOD(exports, "stacktree_by_bytes_total", Memoro_StackTreeByBytesTotal);
  NODE_SET_METHOD(exports, "stacktree_by_peak_waste", Memoro_StackTreeByPeakWaste);
  NODE_SET_METHOD(exports, "stacktree_by_numallocs",
                  Memoro_StackTreeByNumAllocs);
  NODE_SET_METHOD(exports, "trace_limit", Memoro_TraceLimit);
}

NODE_MODULE(memoro, init)
