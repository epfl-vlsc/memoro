#include <cstdio>

#include "TraceStatWriter.hpp"

TraceStatWriter::TraceStatWriter(const std::vector<Trace> &traces) {
  stats_size = traces.size();
  tracestat_buf = new TraceStat[stats_size];

  // Copy traces and aggregate sizes
  for (size_t i = 0; i < traces.size(); ++i) {
    TraceStat &ts = tracestat_buf[i];
    ts.Init(traces[i], traces_size, types_size, aggs_size);

    traces_size += ts.trace.size();
    types_size += ts.type.size();
    aggs_size += ts.aggregate_size;
  }

  trace_buf = new char[traces_size];
  type_buf = new char[types_size];
  agg_buf = new TimeValue[aggs_size];

  // Copy stack traces, types and aggregates
  uint64_t trace_index = 0, type_index = 0, agg_index = 0;
  for (const auto &t : traces) {
    memcpy(trace_buf + trace_index, t.trace.data(), t.trace.size() * sizeof(char));
    trace_index += t.trace.size();

    memcpy(type_buf + type_index, t.type.data(), t.type.size() * sizeof(char));
    type_index += t.type.size();

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

  
bool TraceStatWriter::WriteLargeBufferToFile(const char *buffer, const uint64_t buffer_size, FILE *outfile) {
  uint64_t total_written = 0;

  while (total_written < buffer_size) {
    // TODO: check IO error
    total_written += fwrite(buffer + total_written, sizeof(char), buffer_size - total_written, outfile);
  }

  return true;
}

void TraceStatWriter::Write(const char *outpath) {
  FILE *outfile = fopen(outpath, "w");
  if (outfile == nullptr)
    throw "failed to open out file";

  printf("Writing %llu stats, %llu trace bytes, %llu type bytes, %llu samples\n",
      stats_size, traces_size, types_size, aggs_size);

  TraceStatHeader header{
    stats_size, traces_size, types_size, aggs_size
  };

  WriteLargeBufferToFile((char*)&header, sizeof(TraceStatHeader), outfile);
  WriteLargeBufferToFile((char*)tracestat_buf, sizeof(TraceStat) * stats_size, outfile);
  WriteLargeBufferToFile((char*)trace_buf, sizeof(char) * traces_size, outfile);
  WriteLargeBufferToFile((char*)type_buf, sizeof(char) * types_size, outfile);
  WriteLargeBufferToFile((char*)agg_buf, sizeof(TimeValue) * aggs_size, outfile);

  fclose(outfile);
}
