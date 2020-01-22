#pragma once

#include "Memoro.hpp"

class TraceStatWriter {
  std::unique_ptr<TraceStat[]> tracestat_buf_;
  std::unique_ptr<char[]> trace_buf_, type_buf_;
  std::unique_ptr<TimeValue[]> agg_buf_;

  uint64_t stats_size_ = 0, traces_size_ = 0, types_size_ = 0, aggs_size_ = 0;

  public:
  TraceStatWriter(const std::vector<Trace> &traces);
  
  void Write(std::ostream &outfile);
};
