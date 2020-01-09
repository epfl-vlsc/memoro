#pragma once

#include <stdio.h>
#include <stdint.h>
#include <cstddef>
#include <iostream>


#include "Memoro.hpp"

class TraceStatReader {
  private:
    TraceStatHeader header_;
    TraceStat *stats_base_;

    TimeValue *aggregates_base_;

  public:
    TraceStatReader(FILE *tracestatfile) {
      fread(&header_, sizeof(TraceStatHeader), 1, tracestatfile);

      printf("Reading %llu stats, %llu trace bytes, %llu type bytes, %llu samples\n",
          header_.stats_count, header_.traces_size, header_.types_size, header_.aggregates_count);

      // Compute size of ecah parts
      size_t stats_size = header_.stats_count * sizeof(TraceStat);
      size_t aggregates_size = header_.aggregates_count * sizeof(TimeValue);
      size_t total_size = stats_size + header_.traces_size + header_.types_size + aggregates_size;

      // Read everything
      // TODO: Check return value
      // TODO: Read in multiple chunks if too large
      char *data = new char[total_size];
      fread(data, total_size, 1, tracestatfile);

      // Compute pointers of each parts
      stats_base_ = (TraceStat*)data;
      char *traces_base = data + stats_size;
      char *types_base = traces_base + header_.traces_size;
      aggregates_base_ = (TimeValue*)(types_base + header_.types_size);

      // Fix internal offsets
      for (uint64_t i = 0; i < header_.stats_count; ++i)
        stats_base_[i].Init(traces_base, types_base, aggregates_base_);
    }

    ~TraceStatReader() { delete stats_base_; }

    TraceStat* GetStats() { return stats_base_; }
    uint64_t GetStatsSize() { return header_.stats_count; }

    TimeValue* GetAggregates() { return aggregates_base_; }
    size_t GetAggregatesSize() { return header_.aggregates_count; }
};
