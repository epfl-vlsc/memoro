
//===-- memoro.cc ------------------------------------------------===//
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

#include <dirent.h>
#include "memoro.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <vector>
#include "pattern.h"
#include "stacktree.h"
#include <string.h>
#include <string>
#include <limits>

#include "dataset.hpp"
#include "dataset_full.hpp"
#include "dataset_stats.hpp"

namespace memoro {

using namespace std;

// its just easier this way ...
static unique_ptr<Dataset> theDataset;

bool SetDataset(const std::string& dir_path, const string& trace_file, const string& chunk_file,
                string& msg) {
  theDataset = make_unique<DatasetFull>(dir_path, trace_file, chunk_file, msg);
  return true;
}

bool SetDataset(const std::string& dir_path, const string& stats_file, string& msg) {
  theDataset = make_unique<DatasetStats>(dir_path, stats_file, msg);
  return true;
}

void AggregateAll(std::vector<TimeValue>& values) {
  theDataset->AggregateAll(values);
}

void GlobalInformation(struct GlobalInfo& info) { theDataset->GlobalInfo(info); }

uint64_t MaxAggregate() { return theDataset->MaxAggregate(); }

// TODO maybe these should just default to the filtered numbers?
uint64_t MaxTime() { return theDataset->MaxTime(); }

uint64_t MinTime() { return theDataset->MinTime(); }

uint64_t FilterMaxTime() { return theDataset->FilterMaxTime(); }

uint64_t FilterMinTime() { return theDataset->FilterMinTime(); }

void SetTraceKeyword(const std::string& keyword) {
  // will only include traces that contain this keyword
  theDataset->SetTraceFilter(keyword);
}

void SetTypeKeyword(const std::string& keyword) {
  // will only include traces that contain this keyword
  theDataset->SetTypeFilter(keyword);
}

void Traces(std::vector<TraceValue>& traces) { theDataset->Traces(traces); }

void AggregateTrace(std::vector<TimeValue>& values, int trace_index) {
  theDataset->AggregateTrace(values, trace_index);
}

void TraceChunks(std::vector<Chunk*>& chunks, int trace_index, int chunk_index,
                 int num_chunks) {
  theDataset->TraceChunks(chunks, trace_index, chunk_index, num_chunks);
}

void SetFilterMinMax(uint64_t min, uint64_t max) {
  theDataset->SetFilterMinMax(min, max);
}

void TraceFilterReset() { theDataset->TraceFilterReset(); }

void TypeFilterReset() { theDataset->TypeFilterReset(); }

void FilterMinMaxReset() { theDataset->FilterMinMaxReset(); }

uint64_t Inefficiencies(int trace_index) {
  return theDataset->Inefficiences(trace_index);
}

uint64_t GlobalAllocTime() { return theDataset->GlobalAllocTime(); }

void StackTreeObject(const v8::FunctionCallbackInfo<v8::Value>& args) {
  theDataset->StackTreeObject(args);
}

void StackTreeAggregate(std::function<double(const Trace* t)> f, const string& key) {
  theDataset->StackTreeAggregate(f);
  theDataset->StackTreeAggregate(key);
}

void TraceLimit(uint64_t limit) {
  theDataset->TraceLimit(limit);
}

}  // namespace memoro
