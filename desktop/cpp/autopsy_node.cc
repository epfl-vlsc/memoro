
#include <node.h>
#include <iostream>
#include <v8.h>
#include "autopsy.h" 

using namespace v8;

void Autopsy_SetDataset(const v8::FunctionCallbackInfo<v8::Value> & args) {
  Isolate* isolate = args.GetIsolate();

  v8::String::Utf8Value s(args[0]);
  std::string file_path(*s);

  SetDataset(file_path);
  std::cout << file_path << std::endl;
}

void init(Handle <Object> exports, Handle<Object> module) {
  NODE_SET_METHOD(exports, "set_dataset", Autopsy_SetDataset);

}

NODE_MODULE(autopsy, init)
