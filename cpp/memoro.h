
//===-- memoro.h ------------------------------------------------===//
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
#include <cstdint>
#include <functional>
#include <string>
#include <vector>


// honestly this entire API and inteface to JS needs to be 
// redesigned. 

namespace memoro {

#define MAX_POINTS 700
#define VERSION_MAJOR 0
#define VERSION_MINOR 1

enum LoadingState : uint32_t {
  LoadData = 0,
  Parsing,
  Sorting,
  Building,
  Aggregating,
  Done
};

struct __attribute__((packed)) Header {
  uint8_t version_major = 0;
  uint8_t version_minor = 1;
  uint8_t compression_type = 0;
  uint16_t segment_start = 0;
  uint32_t index_size = 0;
};

struct __attribute__((packed)) Chunk {
  uint8_t num_reads = 0;
  uint8_t num_writes = 0;
  uint8_t allocated = 0;
  uint8_t multi_thread = 0;
  uint32_t stack_index = 0;  // used for file writer
  uint64_t size = 0;
  uint64_t timestamp_start = 0;
  uint64_t timestamp_end = 0;
  uint64_t timestamp_first_access = 0;
  uint64_t timestamp_last_access = 0;
  uint64_t alloc_call_time = 0;
  uint32_t access_interval_low = 0;
  uint32_t access_interval_high = 0;
};

struct TimeValue {
  uint64_t time;
  int64_t value;

  friend bool operator<(const TimeValue& a, const TimeValue& b) {
    return a.time > b.time;
  }
};

struct Trace {
  std::string trace;
  std::string type;
  bool filtered = false;
  bool type_filtered = false;
  uint64_t max_aggregate = 0;
  std::vector<Chunk*> chunks;
  std::vector<TimeValue> aggregate;
  uint64_t inefficiencies = 0;
  uint64_t alloc_time_total = 0;

  float usage_score;
  float lifetime_score;
  float useful_lifetime_score;
};

// API will return a list of these to Node layer
// changing filters will invalidate the indices
struct TraceValue {
  std::string_view trace;
  std::string_view type;
  int trace_index;
  int chunk_index;
  int num_chunks;
  uint64_t alloc_time_total;
  uint64_t max_aggregate;

  float usage_score;
  float lifetime_score;
  float useful_lifetime_score;
};

struct GlobalInfo {
    uint64_t num_traces, num_chunks;
    uint64_t aggregate_max;
    uint64_t total_time, alloc_time;
    float usage_avg, usage_var;
    float lifetime_avg, lifetime_var;
    float useful_lifetime_avg, useful_lifetime_var;
};

// set the current dataset file, returns dataset stats (num traces, min/max
// times)
bool SetDataset(const std::string& file_path, const std::string& trace_file,
                const std::string& chunk_file, std::string& msg);
bool SetDataset(const std::string& dir_path, const std::string& stats_file, std::string& msg);

// add a timestamp interval filter
void SetMinMaxTime(uint64_t max, uint64_t min);
void RemoveMinMaxTime();

// add a trace keyword filter, filtering traces not containing the keyword
// returns new number of active traces
void SetTraceKeyword(const std::string& keyword);
void RemoveTraceKeyword(const std::string& keyword);
void TraceFilterReset();

void SetTypeKeyword(const std::string& keyword);
void TypeFilterReset();

// fill times and values with 1000 aggregate data points from whole dataset
// respects filters
void AggregateAll(std::vector<TimeValue>& values);

// aggregate chunks of a single stacktrace
void AggregateTrace(std::vector<TimeValue>& values, int trace_index);

// get the specified number of chunks starting at the specified indexes
// respects filters, returns empty if all filtered
void TraceChunks(std::vector<Chunk*>& chunks, int trace_index, int chunk_index,
                 int num_chunks);

// build list of traces
void Traces(std::vector<TraceValue>& traces);

void SetFilterMinMax(uint64_t min, uint64_t max);
void FilterMinMaxReset();

uint64_t MaxTime();
uint64_t MinTime();
uint64_t FilterMaxTime();
uint64_t FilterMinTime();
uint64_t GlobalAllocTime();

uint64_t Inefficiencies(int trace_index);

uint64_t MaxAggregate();

void StackTreeObject(const v8::FunctionCallbackInfo<v8::Value>& args);
void StackTreeAggregate(std::function<double(const Trace* t)> f, const std::string& key);

}  // namespace memoro
