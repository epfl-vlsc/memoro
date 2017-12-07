
#include <node.h>
#include <v8.h>
#include <uv.h>
#include <iostream>
#include <algorithm>
#include "autopsy.h" 
#include "pattern.h"

using namespace v8;
using namespace autopsy;

// primary interface to node/JS here
// technique / org lifted from https://github.com/freezer333/nodecpp-demo
// more or less the interface just converts to/from in memory data structures (autopsy.h/cc)
// to V8 JS objects that are passed back to the JS gui layer

struct LoadDatasetWork {
  uv_work_t  request;
  Persistent<Function> callback;

  std::string dir_path;
  std::string trace_path;
  std::string chunk_path;
  std::string msg;
  bool result;
};

static void LoadDatasetAsync(uv_work_t *req)
{
    LoadDatasetWork *work = static_cast<LoadDatasetWork *>(req->data);

    work->result = SetDataset(work->dir_path, work->trace_path, work->chunk_path, work->msg);
}

static void LoadDatasetAsyncComplete(uv_work_t *req,int status)
{
    Isolate * isolate = Isolate::GetCurrent();

    // Fix for Node 4.x - thanks to https://github.com/nwjs/blink/commit/ecda32d117aca108c44f38c8eb2cb2d0810dfdeb
    v8::HandleScope handleScope(isolate);

    LoadDatasetWork *work = static_cast<LoadDatasetWork *>(req->data);

    Local<Object> result = Object::New(isolate);
    result->Set(String::NewFromUtf8(isolate, "message"), 
        String::NewFromUtf8(isolate, work->msg.c_str()));
    result->Set(String::NewFromUtf8(isolate, "result"), 
        Boolean::New(isolate, work->result));

    // set up return arguments
    Local<Value> argv[1] = { result };

    // execute the callback
    Local<Function>::New(isolate, work->callback)->Call(isolate->GetCurrentContext()->Global(), 1, argv);

    // Free up the persistent function callback
    work->callback.Reset();
    delete work;
}

void Autopsy_SetDataset(const v8::FunctionCallbackInfo<v8::Value> & args) {
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

  uv_queue_work(uv_default_loop(),&work->request,LoadDatasetAsync,LoadDatasetAsyncComplete);

  args.GetReturnValue().Set(Undefined(isolate));
}

void Autopsy_AggregateAll(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();
  std::vector<TimeValue> values;
  AggregateAll(values);

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < values.size(); i++ ) {
    Local<Object> result = Object::New(isolate);
    result->Set(String::NewFromUtf8(isolate, "ts"), 
                            Number::New(isolate, values[i].time));
    result->Set(String::NewFromUtf8(isolate, "value"), 
                            Number::New(isolate, values[i].value));
    result_list->Set(i, result);
  }

  args.GetReturnValue().Set(result_list);
}

void Autopsy_AggregateTrace(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();
  static std::vector<TimeValue> values;
  values.clear();

  int trace_index = args[0]->NumberValue();

  AggregateTrace(values, trace_index);

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < values.size(); i++ ) {
    Local<Object> result = Object::New(isolate);
    result->Set(String::NewFromUtf8(isolate, "ts"), 
                            Number::New(isolate, values[i].time));
    result->Set(String::NewFromUtf8(isolate, "value"), 
                            Number::New(isolate, values[i].value));
    result_list->Set(i, result);
  }

  args.GetReturnValue().Set(result_list);
}

void Autopsy_TraceChunks(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();
  static std::vector<Chunk*> chunks;
  chunks.clear();

  int trace_index = args[0]->NumberValue();
  int chunk_index = args[1]->NumberValue();
  int num_chunks  = args[2]->NumberValue();

  TraceChunks(chunks, trace_index, chunk_index, num_chunks);

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < chunks.size(); i++ ) {
    Local<Object> result = Object::New(isolate);
    result->Set(String::NewFromUtf8(isolate, "num_reads"), 
                            Number::New(isolate, chunks[i]->num_reads));
    result->Set(String::NewFromUtf8(isolate, "num_writes"), 
                            Number::New(isolate, chunks[i]->num_writes));
    result->Set(String::NewFromUtf8(isolate, "size"), 
                            Number::New(isolate, chunks[i]->size));
    result->Set(String::NewFromUtf8(isolate, "ts_start"), 
                            Number::New(isolate, chunks[i]->timestamp_start));
    result->Set(String::NewFromUtf8(isolate, "ts_end"), 
                            Number::New(isolate, chunks[i]->timestamp_end));
    result->Set(String::NewFromUtf8(isolate, "ts_first"), 
                            Number::New(isolate, chunks[i]->timestamp_first_access));
    result->Set(String::NewFromUtf8(isolate, "ts_last"), 
                            Number::New(isolate, chunks[i]->timestamp_last_access));
    result->Set(String::NewFromUtf8(isolate, "alloc_call_time"), 
                            Number::New(isolate, chunks[i]->alloc_call_time));
    result->Set(String::NewFromUtf8(isolate, "multi_thread"), 
                            Boolean::New(isolate, chunks[i]->multi_thread));
    result->Set(String::NewFromUtf8(isolate, "access_interval_low"), 
                            Number::New(isolate, chunks[i]->access_interval_low));
    result->Set(String::NewFromUtf8(isolate, "access_interval_high"), 
                            Number::New(isolate, chunks[i]->access_interval_high));
    result_list->Set(i, result);
  }

  args.GetReturnValue().Set(result_list);
}

void Autopsy_Traces(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();
  static std::vector<TraceValue> traces; // just reuse this
  traces.clear();
  Traces(traces);

  Local<Array> result_list = Array::New(isolate);
  for (unsigned int i = 0; i < traces.size(); i++ ) {
    Local<Object> result = Object::New(isolate);
    result->Set(String::NewFromUtf8(isolate, "trace"), 
                            String::NewFromUtf8(isolate, traces[i].trace->c_str()));
    result->Set(String::NewFromUtf8(isolate, "type"), 
                            String::NewFromUtf8(isolate, traces[i].type->c_str()));
    result->Set(String::NewFromUtf8(isolate, "trace_index"), 
                            Number::New(isolate, traces[i].trace_index));
    result->Set(String::NewFromUtf8(isolate, "num_chunks"), 
                            Number::New(isolate, traces[i].num_chunks));
    result->Set(String::NewFromUtf8(isolate, "chunk_index"), 
                            Number::New(isolate, traces[i].chunk_index));
    result->Set(String::NewFromUtf8(isolate, "max_aggregate"), 
                            Number::New(isolate, traces[i].max_aggregate));
    result->Set(String::NewFromUtf8(isolate, "alloc_time_total"), 
                            Number::New(isolate, traces[i].alloc_time_total));
    result->Set(String::NewFromUtf8(isolate, "usage_score"), 
                            Number::New(isolate, traces[i].usage_score));
    result->Set(String::NewFromUtf8(isolate, "lifetime_score"), 
                            Number::New(isolate, traces[i].lifetime_score));
    result->Set(String::NewFromUtf8(isolate, "useful_lifetime_score"), 
                            Number::New(isolate, traces[i].useful_lifetime_score));
    result_list->Set(i, result);
  }

  args.GetReturnValue().Set(result_list);
}

void Autopsy_MaxTime(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, MaxTime());

  args.GetReturnValue().Set(retval);
}

void Autopsy_MinTime(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, MinTime());

  args.GetReturnValue().Set(retval);
}

void Autopsy_FilterMaxTime(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, FilterMaxTime());

  args.GetReturnValue().Set(retval);
}

void Autopsy_FilterMinTime(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, FilterMinTime());

  args.GetReturnValue().Set(retval);
}

void Autopsy_MaxAggregate(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, MaxAggregate());

  args.GetReturnValue().Set(retval);
}

void Autopsy_GlobalAllocTime(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, GlobalAllocTime());

  args.GetReturnValue().Set(retval);
}

void Autopsy_SetTraceKeyword(const v8::FunctionCallbackInfo<v8::Value> & args) {

  v8::String::Utf8Value s(args[0]);
  std::string keyword(*s);

  SetTraceKeyword(keyword);
}

void Autopsy_SetTypeKeyword(const v8::FunctionCallbackInfo<v8::Value> & args) {

  v8::String::Utf8Value s(args[0]);
  std::string keyword(*s);

  SetTypeKeyword(keyword);
}

void Autopsy_SetFilterMinMax(const v8::FunctionCallbackInfo<v8::Value> & args) {

  uint64_t t1 = args[0]->NumberValue();
  uint64_t t2 = args[1]->NumberValue();
  SetFilterMinMax(t1, t2);

}

void Autopsy_TraceFilterReset(const v8::FunctionCallbackInfo<v8::Value> & args) {
  TraceFilterReset();
}

void Autopsy_TypeFilterReset(const v8::FunctionCallbackInfo<v8::Value> & args) {
  TypeFilterReset();
}

void Autopsy_FilterMinMaxReset(const v8::FunctionCallbackInfo<v8::Value> & args) {
  FilterMinMaxReset();
}

void Autopsy_Inefficiencies(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  int trace_index = args[0]->NumberValue();
  uint64_t i = Inefficiencies(trace_index);

  // there will be more here eventually, probably
  Local<Object> result = Object::New(isolate);
  result->Set(String::NewFromUtf8(isolate, "unused"), 
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::Unused)));
  result->Set(String::NewFromUtf8(isolate, "write_only"), 
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::WriteOnly)));
  result->Set(String::NewFromUtf8(isolate, "read_only"), 
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::ReadOnly)));
  result->Set(String::NewFromUtf8(isolate, "short_lifetime"), 
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::ShortLifetime)));
  result->Set(String::NewFromUtf8(isolate, "late_free"), 
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::LateFree)));
  result->Set(String::NewFromUtf8(isolate, "early_alloc"), 
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::EarlyAlloc)));
  result->Set(String::NewFromUtf8(isolate, "increasing_allocs"), 
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::IncreasingReallocs)));
  result->Set(String::NewFromUtf8(isolate, "top_percentile_size"), 
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::TopPercentileSize)));
  result->Set(String::NewFromUtf8(isolate, "top_percentile_chunks"), 
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::TopPercentileChunks)));
  result->Set(String::NewFromUtf8(isolate, "multi_thread"), 
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::MultiThread)));
  result->Set(String::NewFromUtf8(isolate, "low_access_coverage"), 
      Boolean::New(isolate, HasInefficiency(i, Inefficiency::LowAccessCoverage)));
  
  args.GetReturnValue().Set(result);
}

void Autopsy_StackTree(const v8::FunctionCallbackInfo<v8::Value> & args) {
  // was hoping to keep all the V8 stuff in this file, oh well ..
  StackTreeObject(args);
}

void Autopsy_StackTreeByBytes(const v8::FunctionCallbackInfo<v8::Value> & args) {

  uint64_t time = args[0]->NumberValue();

  StackTreeAggregate([time](const Trace* t) -> double {
    auto it = find_if(t->aggregate.begin(), t->aggregate.end(), 
        [time](const TimeValue& a) {
          return a.time >= time;
        });
    if (it == t->aggregate.end()) {
      //std::cout << "found no time";
      return (double) 0;
    }
    // take the prev value when our point was actually greater than the selected time
    if (it != t->aggregate.begin() && it->time > time)
      it--;
    //std::cout << "value is " << it->value << " at time " << it->time << " for trace " << t->trace << std::endl;
    return (double)it->value;
  });

}

void Autopsy_StackTreeByNumAllocs(const v8::FunctionCallbackInfo<v8::Value> & args) {

  StackTreeAggregate([](const Trace* t) -> double {
      return (double) t->chunks.size();
    });
}

void init(Handle <Object> exports, Handle<Object> module) {
  NODE_SET_METHOD(exports, "set_dataset", Autopsy_SetDataset);
  NODE_SET_METHOD(exports, "aggregate_all", Autopsy_AggregateAll);
  NODE_SET_METHOD(exports, "max_time", Autopsy_MaxTime);
  NODE_SET_METHOD(exports, "min_time", Autopsy_MinTime);
  NODE_SET_METHOD(exports, "filter_max_time", Autopsy_FilterMaxTime);
  NODE_SET_METHOD(exports, "filter_min_time", Autopsy_FilterMinTime);
  NODE_SET_METHOD(exports, "max_aggregate", Autopsy_MaxAggregate);
  NODE_SET_METHOD(exports, "set_trace_keyword", Autopsy_SetTraceKeyword);
  NODE_SET_METHOD(exports, "set_type_keyword", Autopsy_SetTypeKeyword);
  NODE_SET_METHOD(exports, "traces", Autopsy_Traces);
  NODE_SET_METHOD(exports, "aggregate_trace", Autopsy_AggregateTrace);
  NODE_SET_METHOD(exports, "trace_chunks", Autopsy_TraceChunks);
  NODE_SET_METHOD(exports, "set_filter_minmax", Autopsy_SetFilterMinMax);
  NODE_SET_METHOD(exports, "trace_filter_reset", Autopsy_TraceFilterReset);
  NODE_SET_METHOD(exports, "type_filter_reset", Autopsy_TypeFilterReset);
  NODE_SET_METHOD(exports, "filter_minmax_reset", Autopsy_FilterMinMaxReset);
  NODE_SET_METHOD(exports, "inefficiencies", Autopsy_Inefficiencies);
  NODE_SET_METHOD(exports, "global_alloc_time", Autopsy_GlobalAllocTime);
  NODE_SET_METHOD(exports, "stacktree", Autopsy_StackTree);
  NODE_SET_METHOD(exports, "stacktree_by_bytes", Autopsy_StackTreeByBytes);
  NODE_SET_METHOD(exports, "stacktree_by_numallocs", Autopsy_StackTreeByNumAllocs);
}

NODE_MODULE(autopsy, init)

