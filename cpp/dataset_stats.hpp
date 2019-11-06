#pragma once

#include "dataset.hpp"

#include "memoro-stats/TraceStatReader.hpp"

using namespace std;

namespace memoro {

class DatasetStats : public Dataset {
  private:
    unique_ptr<TraceStatReader> reader_;
    unique_ptr<TimeValue[]> aggregates_;

    std::vector<memoro::Trace> compat_traces_;
    StackTree stack_tree_;

    uint64_t min_time_ = numeric_limits<uint64_t>::max();
    uint64_t max_time_ = numeric_limits<uint64_t>::lowest();
    uint64_t max_aggregate_ = numeric_limits<uint64_t>::lowest();

    uint64_t filter_min_time_ = numeric_limits<uint64_t>::lowest();
    uint64_t filter_max_time_ = numeric_limits<uint64_t>::max();

    uint64_t global_alloc_time_ = 0;

    inline TraceStat& Trace(uint32_t trace_index) {
      return reader_->GetStats()[trace_index];
    }

    inline std::string FixEnding(const string_view& strv) {
      string fixed_str = string(strv);
      fixed_str[fixed_str.size()] = '\0';
      return fixed_str;
    }

    memoro::Trace ConvertTraceCompat(const TraceStat& trace) {
      return {
        .trace = FixEnding(trace.trace),
        .type = FixEnding(trace.type),
        .max_aggregate = trace.total_memory,
        .alloc_time_total = trace.allocations,
        .inefficiencies = trace.peak_wasted_memory,
        .usage_score = trace.usage_score,
        .lifetime_score = trace.lifetime_score,
        .useful_lifetime_score = trace.useful_lifetime_score
      };
    }

  public:
    DatasetStats(const string& dir_path, const string& stats_file, string& msg) {
      cout << "opening " << stats_file << endl;
      FILE *stats_fd = fopen(stats_file.c_str(), "r");
      if (stats_fd == nullptr) {
        msg = "failed to open file " + stats_file;
        return;
      }

      reader_ = make_unique<TraceStatReader>(stats_fd);

      fclose(stats_fd);

      compat_traces_.reserve(reader_->GetStatsSize());

      // Collect some metric values
      min_time_ = 0; // TODO: Use the actual min
      for (uint64_t i{0}; i < reader_->GetStatsSize(); ++i) {
        const auto& trace = Trace(i);

        global_alloc_time_ += trace.wasted_time;

        for (uint64_t j{0}; j < trace.aggregate_size; ++j) {
          const auto& agg = trace.aggregate[j];

          max_time_ = max(max_time_, agg.time);
          max_aggregate_ = max(max_aggregate_, agg.value);
        }

        compat_traces_.emplace_back(ConvertTraceCompat(trace));
      }

      stack_tree_.SetTraces(compat_traces_);

      size_t aggs_size = reader_->GetAggregatesSize();
      aggregates_ = make_unique<TimeValue[]>(aggs_size);
      memcpy(aggregates_.get(), reader_->GetAggregates(), aggs_size * sizeof(TimeValue));
      sort(aggregates_.get(), aggregates_.get() + aggs_size);
    }

    virtual void AggregateAll(vector<TimeValue>& values) {
      size_t aggs_size = reader_->GetAggregatesSize();

      float index = 0;
      float interval = (float)max(1.0, (double)aggs_size / (double)MAX_POINTS);

      values.clear();
      values.reserve(aggs_size);
      for (float i{0}; i < aggs_size; i += interval) {
        const auto& agg = aggregates_[(uint64_t)i];
        values.push_back({agg.time, (int64_t)agg.value});
      }
    }

    // Return the aggregate of the given trace
    virtual void AggregateTrace(vector<TimeValue>& values, uint32_t trace_index) {
      const auto &trace = Trace(trace_index);

      values.clear();
      values.reserve(trace.aggregate_size);
      for (uint16_t i{0}; i < trace.aggregate_size; ++i) {
        const auto& agg = trace.aggregate[i];
        values.push_back({agg.time, (int64_t)agg.value});
      }
    }

    virtual void Traces(vector<TraceValue>& traces) {
      uint64_t stats_size = reader_->GetStatsSize();

      traces.reserve(stats_size);
      for (uint32_t i{0}; i < stats_size; ++i) {
        const TraceStat& trace = Trace(i);

        TraceValue tv;
        tv.trace = FixEnding(trace.trace);
        tv.type = FixEnding(trace.type);
        tv.trace_index = i;
        tv.chunk_index = 0;
        tv.num_chunks = trace.allocations;
        tv.alloc_time_total = trace.wasted_time;
        tv.max_aggregate = trace.peak_wasted_memory; // TODO: should be peak_memory
        tv.usage_score = trace.usage_score;
        tv.lifetime_score = trace.lifetime_score;
        tv.useful_lifetime_score = trace.useful_lifetime_score;

        traces.emplace_back(std::move(tv));
      }
    }

    virtual void TraceChunks(vector<Chunk*>& chunks, uint32_t trace_index, uint64_t chunk_index, uint64_t num_chunks) {
      chunks.clear(); // Unsupported by DatasetStats
    }

    virtual void GlobalInfo(struct GlobalInfo& info) {
      uint64_t num_chunks = 0, num_traces = reader_->GetStatsSize();

      // Sum metrics for avg and var
      float total_usage = 0, total_usage_sq = 0;
      float total_lifetime = 0, total_lifetime_sq = 0;
      float total_useful_lifetime = 0, total_useful_lifetime_sq = 0;

      for (size_t i = 0; i < num_traces; ++i) {
        const TraceStat& t = Trace(i);

        total_usage += t.usage_score;
        total_usage_sq += t.usage_score * t.usage_score;

        total_lifetime += t.lifetime_score;
        total_lifetime_sq += t.lifetime_score * t.lifetime_score;

        total_useful_lifetime += t.useful_lifetime_score;
        total_useful_lifetime_sq += t.useful_lifetime_score * t.useful_lifetime_score;

        num_chunks += t.allocations;
      }

      // Compute avg
      // avg = \sum x / n
      float usage_avg = total_usage / num_traces;
      float lifetime_avg = total_lifetime / num_traces;
      float useful_lifetime_avg = total_useful_lifetime / num_traces;

      // Comput var
      // var = - avg^2 + \sum x^2 / n
      float usage_var = total_usage_sq / num_traces - usage_avg * usage_avg;
      float lifetime_var = total_lifetime_sq / num_traces - lifetime_avg * lifetime_avg;
      float useful_lifetime_var = total_useful_lifetime_sq / num_traces - useful_lifetime_avg * useful_lifetime_avg;

      uint64_t total_time = max_time_ - min_time_;

      info = {
        num_traces, num_chunks,
        max_aggregate_,
        total_time, global_alloc_time_,
        usage_avg, usage_var,
        lifetime_avg, lifetime_var,
        useful_lifetime_avg, useful_lifetime_var,
      };
    }

    virtual uint64_t Inefficiences(uint32_t trace_index) {
      return 0; // TODO: Compute those in memoro-stats
    }

    virtual uint64_t MaxTime() { return max_time_; }
    virtual uint64_t MinTime() { return min_time_; }
    virtual uint64_t MaxAggregate() { return max_aggregate_; }
    virtual uint64_t GlobalAllocTime() { return global_alloc_time_; }
    virtual uint64_t FilterMaxTime() { return filter_max_time_; }
    virtual uint64_t FilterMinTime() { return filter_min_time_; }

    virtual void SetFilterMinMax(uint64_t min, uint64_t max) {
      if (min >= max) return;
      if (max > max_time_) return;

      filter_min_time_ = min;
      filter_max_time_ = max;
    }

    virtual void FilterMinMaxReset() {
      filter_min_time_ = min_time_;
      filter_max_time_ = max_time_;
    }

    virtual void SetTraceFilter(const string& needle) {}
    virtual void SetTypeFilter(const string& needle) {}
    virtual void TraceFilterReset() {}
    virtual void TypeFilterReset() {}

    virtual void StackTreeObject(const v8::FunctionCallbackInfo<v8::Value>& args) {
      stack_tree_.V8Objectify(args);
    }

    virtual void StackTreeAggregate(function<double(const memoro::Trace* t)> f) {
      // NOP: Not supported by DatasetStats
    }

    virtual void StackTreeAggregate(const string& key) {
      if (key == "ByBytesInTime")
        return; // NOP: Not supported by DatasetStats
      else if (key == "ByBytesTotal")
        stack_tree_.Aggregate([](const memoro::Trace* t) -> double { return t->max_aggregate; });
      else if (key == "ByNumAllocs")
        stack_tree_.Aggregate([](const memoro::Trace* t) -> double { return t->alloc_time_total; });
      else if (key == "ByPeakWaste")
        stack_tree_.Aggregate([](const memoro::Trace* t) -> double { return t->inefficiencies; });
    }

    virtual void TraceLimit(uint64_t limit) {
      stack_tree_.SetLimit(limit);
    }
};

} // namespace memoro
