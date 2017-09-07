
#include <node.h>
#include <iostream>
#include <v8.h>
#include "autopsy.h" 

using namespace v8;

void Autopsy_SetDataset(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  v8::String::Utf8Value s(args[0]);
  uint32_t num_traces;
  std::string file_path(*s);

  SetDataset(file_path, num_traces);
  
  Local<Number> retval = v8::Number::New(isolate, num_traces);
  args.GetReturnValue().Set(retval);
}

void Autopsy_AggregateAll(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();
  std::vector<TimeValue> values;
  AggregateAll(values);
  //std::cout << "got " << values.size() << " time points!!\n";

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

void Autopsy_MaxAggregate(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  Local<Number> retval = v8::Number::New(isolate, MaxAggregate());

  args.GetReturnValue().Set(retval);
}

void Autopsy_SetTraceKeyword(const v8::FunctionCallbackInfo<v8::Value> & args) {

  v8::String::Utf8Value s(args[0]);
  std::string keyword(*s);

  SetTraceKeyword(keyword);
}

void init(Handle <Object> exports, Handle<Object> module) {
  NODE_SET_METHOD(exports, "set_dataset", Autopsy_SetDataset);
  NODE_SET_METHOD(exports, "aggregate_all", Autopsy_AggregateAll);
  NODE_SET_METHOD(exports, "max_time", Autopsy_MaxTime);
  NODE_SET_METHOD(exports, "min_time", Autopsy_MinTime);
  NODE_SET_METHOD(exports, "max_aggregate", Autopsy_MaxAggregate);
  NODE_SET_METHOD(exports, "set_trace_keyword", Autopsy_SetTraceKeyword);

}

NODE_MODULE(autopsy, init)
