#include <cstdio>
#include <ostream>

#include "TraceStatWriter.hpp"

TraceStatWriter::TraceStatWriter(const std::vector<Trace> &traces) {
  stats_size = traces.size();
  tracestat_buf = new TraceStat[stats_size];

  // Copy traces and aggregate sizes
  for (size_t i = 0; i < traces.size(); ++i) {
    TraceStat &ts = tracestat_buf[i];
    ts.Init(traces[i], traces_size, types_size, aggs_size);

    // +1 for '\0' character
    traces_size += ts.trace.size() + 1;
    types_size += ts.type.size() + 1;
    aggs_size += ts.aggregate_size;
  }

  trace_buf = new char[traces_size];
  type_buf = new char[types_size];
  agg_buf = new TimeValue[aggs_size];

  // Copy stack traces, types and aggregates
  uint64_t trace_index = 0, type_index = 0, agg_index = 0;
  for (const auto &t : traces) {
    size_t trace_size = t.trace.size() + 1;
    memcpy(trace_buf + trace_index, t.trace.data(), trace_size * sizeof(char));
    trace_index += trace_size;

    size_t type_size = t.type.size() + 1;
    memcpy(type_buf + type_index, t.type.data(), type_size * sizeof(char));
    type_index += type_size;

    memcpy(agg_buf + agg_index, t.samples.data(), t.samples.size() * sizeof(TimeValue));
    agg_index += t.samples.size();
  }
}

TraceStatWriter::~TraceStatWriter() {
  delete[] tracestat_buf;
  delete[] trace_buf;
  delete[] type_buf;
  delete[] agg_buf;
}

void TraceStatWriter::Write(std::ostream &outfile) {
  printf("Writing %llu stats, %llu trace bytes, %llu type bytes, %llu samples\n",
      stats_size, traces_size, types_size, aggs_size);

  TraceStatHeader header{ stats_size, traces_size, types_size, aggs_size };

  outfile.write((char*)&header, sizeof(TraceStatHeader));
  outfile.write((char*)tracestat_buf, stats_size * sizeof(TraceStat));
  outfile.write((char*)trace_buf, traces_size * sizeof(char));
  outfile.write((char*)type_buf, types_size * sizeof(char));
  outfile.write((char*)agg_buf, aggs_size * sizeof(TimeValue));
  outfile.flush();
}
