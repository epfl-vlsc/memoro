#pragma once

#include "Memoro.hpp"

class TraceStatWriter {
  TraceStat *tracestat_buf;
  char *trace_buf, *type_buf;
  TimeValue *agg_buf;

  uint64_t stats_size = 0, traces_size = 0, types_size = 0, aggs_size = 0;

  public:
  TraceStatWriter(const std::vector<Trace> &traces);
  ~TraceStatWriter();
  
  void Write(std::ostream &outfile);
};
