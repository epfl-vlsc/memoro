#pragma once

#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <istream>

#include "Memoro.hpp"

class TraceStatReader {
  private:
    TraceStatHeader header_;
    std::unique_ptr<char[]> data_;

    TraceStat *stats_base_;
    TimeValue *aggregates_base_;

  public:
    TraceStatReader(std::istream &tracestatfile) {
      tracestatfile.read((char*)&header_, sizeof(TraceStatHeader));

      printf("Reading %llu stats, %llu trace bytes, %llu type bytes, %llu samples\n",
          header_.stats_count, header_.traces_size, header_.types_size, header_.aggregates_count);

      // Compute size of ecah parts
      size_t stats_size = header_.stats_count * sizeof(TraceStat);
      size_t aggregates_size = header_.aggregates_count * sizeof(TimeValue);
      size_t total_size = stats_size + header_.traces_size + header_.types_size + aggregates_size;

      // Read everything
      // TODO: Check return value
      // TODO: Read in multiple chunks if too large
      data_ = std::make_unique<char[]>(total_size);
      tracestatfile.read(data_.get(), total_size);

      // Compute pointers of each parts
      stats_base_ = (TraceStat*)data_.get();
      char *traces_base = data_.get() + stats_size;
      char *types_base = traces_base + header_.traces_size;
      aggregates_base_ = (TimeValue*)(types_base + header_.types_size);

      // Fix internal offsets
      for (uint64_t i = 0; i < header_.stats_count; ++i)
        stats_base_[i].Init(traces_base, types_base, aggregates_base_);
    }

    TraceStat* GetStats() { return stats_base_; }
    uint64_t GetStatsSize() { return header_.stats_count; }

    TimeValue* GetAggregates() { return aggregates_base_; }
    size_t GetAggregatesSize() { return header_.aggregates_count; }
};
