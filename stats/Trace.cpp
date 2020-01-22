#include "Memoro.hpp"

#include <vector>
#include <queue>

#include <iostream>

using namespace std;

void Trace::Stats() {
  allocations = chunks.size();

  UsageScore();
  LifetimeScore();
  UsefulLifetimeScore();

  PeakWastedMemory();
  SampleValues();
  Aggregate();
}

float Trace::UsageScore() {
  uint64_t used_bytes = 0, total_bytes = 0;

  for (const auto& chunk : chunks) {
    if (chunk.num_writes == 0 && chunk.num_reads == 0)
      continue;

    total_bytes += chunk.size;
    used_bytes  += chunk.access_interval_high - chunk.access_interval_low;
  }

  if (total_bytes == 0)
    return 0;

  usage_score   = double(used_bytes) / double(total_bytes);
  wasted_memory = total_bytes - used_bytes;
  total_memory  = total_bytes;

  return usage_score;
}

float Trace::LifetimeScore() {
  return 0;
}

float Trace::UsefulLifetimeScore() {
  uint64_t active_life = 0, total_life = 0;

  for (const auto& chunk : chunks) {
    total_life  += chunk.timestamp_end - chunk.timestamp_start;
    active_life += chunk.timestamp_last_access - chunk.timestamp_first_access;
  }

  useful_lifetime_score = double(active_life) / double(total_life);

  return useful_lifetime_score;
}

struct STimeValue {
  uint64_t time;
  int64_t value;

  friend bool operator<(const STimeValue& a, const STimeValue& b) {
    return a.time > b.time;
  }
};

uint64_t Trace::PeakWastedMemory() {
  priority_queue<STimeValue> queue;

  int64_t peak_waste = 0, acc_waste = 0;
  uint32_t chunk_index = 0;
  while (chunk_index < chunks.size()) {
    const Chunk &chunk = chunks[chunk_index];

    // A chunk gets freed
    if (!queue.empty() && queue.top().time < chunk.timestamp_start) {
      acc_waste -= queue.top().value;
      /* printf("Released: %llu, Acc: %llu\n", queue.top().value, acc_waste); */
      queue.pop();
    } else { // A chunk gets allocated
      int64_t total_bytes = chunk.size;
      int64_t used_bytes  = chunk.access_interval_high - chunk.access_interval_low;
      int64_t wasted_bytes= total_bytes - used_bytes;

      if (wasted_bytes < 0) {
        printf("Chunk=[\n  Index=%u,\n  Allocated=%ld,\n  Accessed=[%d;%d],\n  AccessedRange=%ld,\n  Wasted=%ld,\n]\n",
            chunk_index, total_bytes, chunk.access_interval_high, chunk.access_interval_low, used_bytes, wasted_bytes);
        ++chunk_index;
        continue;
      }

      acc_waste += wasted_bytes;
      peak_waste = max(acc_waste, peak_waste);
      /* printf("Wasted: %llu, Acc: %llu, Peak: %llu\n", wasted_bytes, acc_waste, peak_waste); */

      queue.push({ chunk.timestamp_end, wasted_bytes });
      ++chunk_index;
    }
  }
  return peak_wasted_memory = peak_waste;
}

void Trace::SampleValues() {
  constexpr uint32_t SAMPLE_MAX_POINTS = MAX_POINTS / 2;
  if (chunks.size() <= SAMPLE_MAX_POINTS)
    return;

  // Using float to improve sampling uniformity
  float interval = (double)chunks.size() / (double)SAMPLE_MAX_POINTS;

  vector<Chunk> samples(SAMPLE_MAX_POINTS);
  for (uint32_t i = 0; i < SAMPLE_MAX_POINTS; ++i)
    samples[i] = chunks[i * interval];

  /* printf("Had %lu chunks, only %lu samples remaining\n", chunks.size(), samples.size()); */
  chunks = move(samples);
}

// The queue in Aggregate need ordering of the TimeValue
bool operator<(const TimeValue& a, const TimeValue& b) {
  return a.time > b.time;
}

void Trace::Aggregate() {
  const auto chunks_count = chunks.size();

  priority_queue<TimeValue> queue;
  vector<TimeValue> samples(2 * chunks_count);

  uint64_t acc = 0;
  uint32_t chunk_index = 0;
  for (uint32_t i = 0; i < (2 * chunks_count); ++i) {
    const Chunk &chunk = chunks[chunk_index];

    if (!queue.empty() && (chunk_index >= chunks.size() || queue.top().time < chunk.timestamp_start)) {
      const TimeValue& top = queue.top();
      queue.pop();

      acc += top.value;
      samples[i] = { top.time, acc };
    } else {
      acc += chunk.size;
      samples[i] = { chunk.timestamp_start, acc };

      queue.push({ chunk.timestamp_end, -chunk.size });
      ++chunk_index;
    }
  }

  /* printf("Had %lu chunks, ends with %lu aggregates\n", chunks.size(), samples.size()); */
  this->chunks.resize(0);
  this->samples = std::move(samples);
}
