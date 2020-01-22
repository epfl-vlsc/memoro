#pragma once

#include <cstdio>
#include <cstdint>
#include <istream>

#include <string>
#include <string_view>
#include <vector>
#include <array>

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define MAX_POINTS 700

struct __attribute__((packed)) Header {
  uint8_t version_major = 0;
  uint8_t version_minor = 1;
  uint8_t compression_type = 0;
  uint16_t segment_start = 0;
  uint32_t index_size = 0;

  Header(size_t index_count) {
    index_size = index_count;
    segment_start = sizeof(Header);
  }

  Header(std::istream &f) {
    f.seekg(0);
    f.read((char*)this, sizeof(Header));
  }

  Header(FILE *f) {
    // TODO: check retvals
    fseek(f, 0, SEEK_SET);
    fread(this, sizeof(Header), 1, f);

    // NodeJS modules have exception disabled
    /* if (version_major != VERSION_MAJOR || */
    /*     version_minor != VERSION_MINOR) */
    /*   throw "Header version mismatch"; */
  }

  void Print() const {
    printf("Version: %u.%u\n", version_major, version_minor);
    printf("Compression scheme: %u\n", compression_type);
    printf("Index start: %u\n", segment_start);
    printf("Index size: %u\n", index_size);
  }
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
  uint64_t time, value;
};

struct Trace {
  std::string trace;
  std::string type;
  std::vector<Chunk> chunks;
  std::vector<TimeValue> samples;

  float usage_score, lifetime_score, useful_lifetime_score;
  uint64_t allocations, total_memory, wasted_memory, peak_wasted_memory, wasted_time;

  Trace(std::string trace, std::string type = {})
    : trace(std::move(trace)), type(std::move(type)) {}

  void Stats();

  float UsageScore();
  float LifetimeScore();
  float UsefulLifetimeScore();

  uint64_t PeakWastedMemory();

  void SampleValues();
  void Aggregate();
};

struct __attribute__((packed)) TraceStatHeader {
  uint64_t stats_count, traces_size, types_size, aggregates_count;
};

struct __attribute__((packed)) TraceStat {
  std::string_view trace, type;

  float usage_score, lifetime_score, useful_lifetime_score;
  uint64_t allocations, total_memory, wasted_memory, peak_wasted_memory, wasted_time;

  uint16_t aggregate_size;
  TimeValue *aggregate;

  // Init TraceStat after deserialization
  void Init(const char *trace_base, const char *type_base, TimeValue* aggregate_base) {
    trace = { trace_base + (size_t)trace.data(), trace.size() };
    type  = { type_base  + (size_t)type.data(),  type.size()  };
    aggregate = aggregate_base + (size_t)aggregate;
  }

  void Init(const Trace& t, uint64_t trace_offset, uint64_t type_offset, uint64_t aggregate_offset) {
    trace = {(char*)trace_offset, t.trace.size()};
    type  = {(char*)type_offset,  t.type.size()};
    aggregate_size = t.samples.size();
    aggregate = (TimeValue*)aggregate_offset;

    usage_score = t.usage_score;
    lifetime_score = t.lifetime_score;
    useful_lifetime_score = t.useful_lifetime_score;
    allocations = t.allocations;
    total_memory = t.total_memory;
    wasted_memory = t.wasted_memory;
    peak_wasted_memory = t.peak_wasted_memory;
    wasted_time = t.wasted_time;
  }

  void Print() const {
    printf("TraceStat=[\n");
    printf("  trace=[%p,%lu],\n", trace.data(), trace.size());
    printf("  type=[%p,%lu],\n", type.data(), type.size());
    printf("  aggregate=[%p,%hu],\n", aggregate, aggregate_size);
    printf("]\n");
  }
};
