
//===-- pattern.cc ------------------------------------------------===//
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

#include "pattern.h"
#include <iostream>
#include <vector>
#include <algorithm>

namespace memoro {

using namespace std;
/*Unused = 0x1,
  WriteOnly = 1 << 1,
  ReadOnly = 1 << 2,
  ShortLifetime = 1 << 3,
  LateFree = 1 << 4,
  EarlyAlloc =  1 << 5,
  IncreasingReallocs = 1 << 6,
  TopPercentile = 1 << 7*/

bool HasInefficiency(uint64_t bitvec, Inefficiency i) { return bool(bitvec & i); }

float UsageScore(std::vector<Chunk*> const& chunks) {
  double sum = 0;
  uint64_t total_bytes = 0;
  for (auto chunk : chunks) {
    if (chunk->num_writes == 0 && chunk->num_reads == 0) { 
      continue;
    }
    sum += double(chunk->access_interval_high - chunk->access_interval_low);
    total_bytes += chunk->size;
  }
  if (sum == 0){
    return 0;
  }
  return float(sum) / float(total_bytes);
}

// divide chunks into groups (regions) where region boundaries are defined by
// `threshold`. If chunk N and chunk N+1 are separated by more than `threshold`,
// they are in different regions.
float LifetimeScore(std::vector<Chunk*> const& chunks, uint64_t threshold) {
  // we avoid `memorizing' regions right now, but may add in the future
  // if we want to annotate in the gui
  double current_lifetime_sum = 0;
  uint32_t current_num_chunks = 0;
  uint64_t region_start_time = chunks[0]->timestamp_start;
  uint64_t region_end_time = chunks[0]->timestamp_end;
  double region_score_total = 0;
  uint32_t num_regions = 0;
  Chunk* prev = nullptr;
  for (auto chunk : chunks) {
    if (prev != nullptr && chunk->timestamp_start - prev->timestamp_start > threshold) {
      // finish this region
      if (prev->timestamp_end > region_end_time) {
        region_end_time = prev->timestamp_end;
      }
      uint64_t region_lifetime = region_end_time - region_start_time;
      double chunk_avg_lifetime =
          current_lifetime_sum / double(current_num_chunks);
      region_score_total += chunk_avg_lifetime / double(region_lifetime);
      num_regions++;

      // start a new one
      current_num_chunks = 0;
      current_lifetime_sum = 0;
      region_start_time = chunk->timestamp_start;
      region_end_time = chunk->timestamp_end;
    }
    current_lifetime_sum += chunk->timestamp_end - chunk->timestamp_start;
    current_num_chunks++;
    if (chunk->timestamp_end > region_end_time) {
      region_end_time = chunk->timestamp_end;
    }
    prev = chunk;
  }

  // finish up last region
  if (prev->timestamp_end > region_end_time) {
    region_end_time = prev->timestamp_end;
  }
  uint64_t region_lifetime = region_end_time - region_start_time;
  double chunk_avg_lifetime = current_lifetime_sum / double(current_num_chunks);
  region_score_total += chunk_avg_lifetime / double(region_lifetime);
  num_regions++;

  return region_score_total / num_regions;
}

float UsefulLifetimeScore(std::vector<Chunk*> const& chunks) {
  double score_sum = 0;
  for (auto chunk : chunks) {
    uint64_t total_life = chunk->timestamp_end - chunk->timestamp_start;
    uint64_t active_life =
        chunk->timestamp_last_access - chunk->timestamp_first_access;
    score_sum += double(active_life) / double(total_life);
  }
  return score_sum / chunks.size();
}

float ReallocScore(std::vector<Chunk*> const& chunks) {
  uint64_t last_size = 0;
  unsigned int current_run = 0, longest_run = 0;
  for (auto chunk : chunks) {
    if (last_size == 0) {
      last_size = chunk->size;
      current_run++;
    } else {
      if (chunk->size >= last_size) {
        last_size = chunk->size;
        current_run++;
      } else {
        longest_run = current_run > longest_run ? current_run : longest_run;
        current_run = 0;
        last_size = chunk->size;
      }
    }
  }
  return 0.0f;
}

uint64_t Detect(std::vector<Chunk*> const& chunks, const PatternParams& params) {
  uint64_t min_lifetime = UINT64_MAX;
  unsigned int total_reads = 0, total_writes = 0;
  bool has_early_alloc = false, has_late_free = false;
  bool has_multi_thread = false;
  bool has_low_access_coverage = false;
  uint64_t last_size = 0;
  unsigned int current_run = 0, longest_run = 0;

  for (auto chunk : chunks) {
    // min lifetime
    uint64_t diff = chunk->timestamp_end - chunk->timestamp_start;
    if (diff < min_lifetime) {
      min_lifetime = diff;
    }

    // total reads, writes
    total_reads += chunk->num_reads;
    total_writes += chunk->num_writes;

    // late free
    if (chunk->timestamp_first_access - chunk->timestamp_start >
        (chunk->timestamp_end - chunk->timestamp_start) / 2) {
      has_early_alloc = true;
    }

    // early alloc
    if (chunk->timestamp_end - chunk->timestamp_last_access >
        (chunk->timestamp_end - chunk->timestamp_start) / 2) {
      has_late_free = true;
    }

    // increasing alloc sizes
    if (last_size == 0) {
      last_size = chunk->size;
      current_run++;
    } else {
      if (chunk->size >= last_size) {
        last_size = chunk->size;
        current_run++;
      } else {
        longest_run = current_run > longest_run ? current_run : longest_run;
        current_run = 0;
        last_size = chunk->size;
      }
    }
    // multithread
    if (bool(chunk->multi_thread)) {
      has_multi_thread = true;
    }

    if (float(chunk->access_interval_high - chunk->access_interval_low) /
            float(chunk->size) <
        params.access_coverage) {
      has_low_access_coverage = true;
    }
  }

  uint64_t i = 0;
  if (min_lifetime <= params.short_lifetime) {
    i |= Inefficiency::ShortLifetime;
  }
  if (total_reads == 0 || total_writes == 0) {
    if (total_writes > 0) {
      i |= Inefficiency::WriteOnly;
    } else if (total_reads > 0) {
      i |= Inefficiency::ReadOnly;
    } else {
      i |= Inefficiency::Unused;
    }
  }

  if (has_early_alloc) {
    i |= Inefficiency::EarlyAlloc;
  }
  if (has_late_free) {
    i |= Inefficiency::LateFree;
  }

  if (longest_run >= params.alloc_min_run) {
    i |= Inefficiency::IncreasingReallocs;
  }
  if (has_multi_thread) {
    i |= Inefficiency::MultiThread;
  }
  if (has_low_access_coverage) {
    i |= Inefficiency::LowAccessCoverage;
  }

  return i;
}

void CalculatePercentilesChunk(std::vector<Trace>& traces,
                               const PatternParams& params) {
  float percentile = params.percentile;
  auto index = int(percentile * traces.size());
  for (unsigned int i = index; i < traces.size(); i++) {
    traces[i].inefficiencies |= Inefficiency::TopPercentileChunks;
  }
}

void CalculatePercentilesSize(std::vector<Trace>& traces,
                              const PatternParams& params) {
  // we need a different sort order but cannot mutate traces like this
  // since it will invalidate chunk parent indexes
  using TVal = pair<uint64_t, unsigned int> ;
  vector<TVal> t;
  t.reserve(traces.size());
  for (int i = 0; i < traces.size(); i++) {
    t.emplace_back(traces[i].max_aggregate, i);
  }
  std::sort(t.begin(), t.end(),
            [](TVal const& a, TVal const& b) { return a.first < b.first; });

  float percentile = params.percentile;
  auto index = int(percentile * traces.size());
  for (unsigned int i = index; i < t.size(); i++) {
    traces[t[i].second].inefficiencies |= Inefficiency::TopPercentileSize;
  }
}

}  // namespace memoro
