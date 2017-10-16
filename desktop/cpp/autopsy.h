
#ifndef AUTOPSY_H
#define AUTOPSY_H

#include <string>
#include <cstdint>
#include <vector>
#include <functional>
#include <v8.h>

namespace autopsy {

enum LoadingState : uint32_t {
  LoadData = 0,
  Parsing,
  Sorting,
  Building,
  Aggregating,
  Done
};

struct __attribute__((packed)) Chunk {
  uint8_t num_reads = 0;
  uint8_t num_writes = 0;
  uint8_t allocated = 0;
  uint8_t multi_thread;
  uint32_t stack_index = 0; // used for file writer
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
  std::string* trace;
  std::string* type;
  int trace_index;
  int chunk_index;
  int num_chunks;
  uint64_t alloc_time_total;
  uint64_t max_aggregate;
  
  float usage_score;
  float lifetime_score;
  float useful_lifetime_score;
};

// set the current dataset file, returns dataset stats (num traces, min/max times)
bool SetDataset(std::string& file_path, std::string& trace_file, 
    std::string& chunk_file, std::string& msg);

// add a timestamp interval filter
void SetMinMaxTime(uint64_t max, uint64_t min);
void RemoveMinMaxTime();

// add a trace keyword filter, filtering traces not containing the keyword
// returns new number of active traces
void SetTraceKeyword(std::string& keyword);
void RemoveTraceKeyword(std::string& keyword);
void TraceFilterReset();

void SetTypeKeyword(std::string& keyword);
void TypeFilterReset();

// fill times and values with 1000 aggregate data points from whole dataset
// respects filters
void AggregateAll(std::vector<TimeValue>& values);

// aggregate chunks of a single stacktrace
void AggregateTrace(std::vector<TimeValue>& values, int trace_index);

// get the specified number of chunks starting at the specified indexes
// respects filters, returns empty if all filtered
void TraceChunks(std::vector<Chunk*>& chunks, int trace_index, int chunk_index, int num_chunks);

// return list of traces
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

void StackTreeObject(const v8::FunctionCallbackInfo<v8::Value> & args);
void StackTreeAggregate(std::function<double (const Trace* t)> f);

} // namespace

#endif


