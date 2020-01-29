#include <cstdio>
#include <ostream>

#include "TraceStatWriter.hpp"

TraceStatWriter::TraceStatWriter(const std::vector<Trace> &traces) {
  stats_size_ = traces.size();
  tracestat_buf_ = std::make_unique<TraceStat[]>(stats_size_);

  // Copy traces and aggregate sizes
  for (size_t i = 0; i < traces.size(); ++i) {
    TraceStat &ts = tracestat_buf_[i];
    ts.Init(traces[i], traces_size_, types_size_, aggs_size_);

    // +1 for '\0' character
    traces_size_ += ts.trace.size() + 1;
    types_size_ += ts.type.size() + 1;
    aggs_size_ += ts.aggregate_size;
  }

  trace_buf_ = std::make_unique<char[]>(traces_size_);
  type_buf_ = std::make_unique<char[]>(types_size_);
  agg_buf_ = std::make_unique<TimeValue[]>(aggs_size_);

  // Copy stack traces, types and aggregates
  uint64_t trace_index = 0, type_index = 0, agg_index = 0;
  for (const auto &t : traces) {
    size_t trace_size = t.trace.size() + 1;
    memcpy(trace_buf_.get() + trace_index, t.trace.data(), trace_size * sizeof(char));
    trace_index += trace_size;

    size_t type_size = t.type.size() + 1;
    memcpy(type_buf_.get() + type_index, t.type.data(), type_size * sizeof(char));
    type_index += type_size;

    memcpy(agg_buf_.get() + agg_index, t.samples.data(), t.samples.size() * sizeof(TimeValue));
    agg_index += t.samples.size();
  }
}

void TraceStatWriter::Write(std::ostream &outfile) {
  printf("Writing %lu stats, %lu trace bytes, %lu type bytes, %lu samples\n",
      stats_size_, traces_size_, types_size_, aggs_size_);

  outfile << TraceStatHeader{ stats_size_, traces_size_, types_size_, aggs_size_ };

  outfile.write((char*)tracestat_buf_.get(), stats_size_ * sizeof(TraceStat));
  outfile.write((char*)trace_buf_.get(), traces_size_ * sizeof(char));
  outfile.write((char*)type_buf_.get(), types_size_ * sizeof(char));
  outfile.write((char*)agg_buf_.get(), aggs_size_ * sizeof(TimeValue));
  outfile.flush();
}
