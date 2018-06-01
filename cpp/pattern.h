
#pragma once 

#include "memoro.h"
#include <vector>

namespace memoro {

enum Inefficiency : uint64_t {
  Unused = 0x1,
  WriteOnly = 1 << 1,
  ReadOnly = 1 << 2,
  ShortLifetime = 1 << 3,
  LateFree = 1 << 4,
  EarlyAlloc =  1 << 5,
  IncreasingReallocs = 1 << 6,
  TopPercentileChunks = 1 << 7,
  TopPercentileSize = 1 << 8,
  MultiThread = 1 << 9,
  LowAccessCoverage = 1 << 10
};

struct PatternParams {
  unsigned int short_lifetime = 1000000; // in ns currently
  unsigned int alloc_min_run = 4;
  float percentile = 0.9f;
  float access_coverage = 0.5f;
};

bool HasInefficiency(uint64_t bitvec, Inefficiency i);

float UsageScore(std::vector<Chunk*> const& chunks);
// threshold typically 1% of program lifetime
float LifetimeScore(std::vector<Chunk*> const& chunks, uint64_t threshold);
float UsefulLifetimeScore(std::vector<Chunk*> const& chunks);

// returns bit vector of inefficiency
uint64_t Detect(std::vector<Chunk*> const& chunks, PatternParams& params);

// mutates traces vector elements
// requires sorted traces by num chunks
void CalculatePercentilesChunk(std::vector<Trace>& traces, PatternParams& params);

// requires sorted traces by max agg
void CalculatePercentilesSize(std::vector<Trace>& traces, PatternParams& params);

}
