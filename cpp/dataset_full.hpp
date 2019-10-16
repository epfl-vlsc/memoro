#pragma once

#include "dataset.hpp"

using namespace std;

namespace memoro {

class DatasetFull : public Dataset {
 public:
  DatasetFull(const string& dir_path, const string& trace_file, const string& chunk_file, string& msg) {
    if (chunk_ptr_ != nullptr) delete[] chunk_ptr_;
    traces_.clear();
    min_time_ = UINT64_MAX;
    aggregates_.clear();
    trace_filters_.clear();
    global_alloc_time_ = 0;

    if (!InitTypeData(dir_path, msg)) {
      return;
    }

    cout << "opening " << trace_file << endl;
    // fopen trace file, build traces array
    FILE* trace_fd = fopen(trace_file.c_str(), "r");
    if (trace_fd == NULL) {
      msg = "failed to open file " + trace_file;
      return;
    }
    Header header;
    fread(&header, sizeof(Header), 1, trace_fd);

    if (header.version_major != VERSION_MAJOR ||
        header.version_minor != VERSION_MINOR) {
      msg = "Header version mismatch in " + trace_file +
            ". \
               Is this a valid trace/chunk file?";
      return;
    }

    vector<uint16_t> index;
    index.resize(header.index_size);
    fread(&index[0], 2, header.index_size, trace_fd);

    cout << "reading " << header.index_size << " traces" << endl;

    traces_.reserve(header.index_size);
    Trace t;
    vector<char> trace_buf;  // used as a resizable buffer
    for (unsigned int i = 0; i < header.index_size; i++) {
      if (index[i] > trace_buf.size()) trace_buf.resize(index[i]);

      fread(&trace_buf[0], index[i], 1, trace_fd);
      t.trace = string(&trace_buf[0], index[i]);
      t.type = Typify(t.trace);


      /*auto ty = type_map_.find(value);
      t.type = ty == type_map_.end() ? string("") : ty->second.second;*/
      traces_.push_back(t);
    }
    fclose(trace_fd);

    // for some reason I can't mmap the file so we open and copy ...
    FILE* chunk_fd = fopen(chunk_file.c_str(), "r");
    // file size produced by sanitizer is buggy and adds a bunch 0 data to
    // end of file. not sure why yet ...
    // int file_size = 0;
    if (chunk_fd == NULL) {
      msg = "failed to open file " + chunk_file;
      return;
    } /*else {
      fseek(chunk_fd, 0L, SEEK_END);
      file_size = ftell(chunk_fd);
      rewind(chunk_fd);
    }*/

    fread(&header, sizeof(Header), 1, chunk_fd);
    if (header.version_major != VERSION_MAJOR ||
        header.version_minor != VERSION_MINOR) {
      msg = "Header version mismatch in " + trace_file +
            ". \
               Is this a valid trace/chunk file?";
      return;
    }

    cout << "reading " << header.index_size << " chunks" << endl;

    index.resize(header.index_size);
    fread(&index[0], 2, header.index_size, chunk_fd);

    num_chunks_ = header.index_size;
    // file_size = file_size - sizeof(Header) - header.index_size*2;
    // cout << " file size now " << file_size << endl;
    chunk_ptr_ = new char[num_chunks_ * sizeof(Chunk)];
    fread(chunk_ptr_, num_chunks_ * sizeof(Chunk), 1, chunk_fd);

    // we can do this because all the fields are the same size (structs)
    chunks_ = (Chunk*)(chunk_ptr_);
    fclose(chunk_fd);
    cout << "done reading\n";

    // sort the chunks makes bin/aggregate easier
    // No need to sort, already handled by Memoro's runtime
    cout << "sorting chunks..." << endl;
    sort(chunks_, chunks_ + num_chunks_, [](Chunk& a, Chunk& b) {
      return a.timestamp_start < b.timestamp_start;
    });

    // set trace structure pointers to their chunks
    cout << "building structures..." << endl;
    min_time_ = 0;
    for (unsigned int i = 0; i < num_chunks_; i++) {
      Trace& t = traces_[chunks_[i].stack_index];
      t.chunks.push_back(&chunks_[i]);
      if (chunks_[i].timestamp_end > max_time_)
        max_time_ = chunks_[i].timestamp_end;
    }
    filter_min_time_ = 0;
    filter_max_time_ = max_time_;

    // populate chunk aggregate vectors
    cout << "aggregating traces ..." << endl;
    uint64_t total_alloc_time = 0;
    for (auto& t : traces_) {
      // aggregate data
      Aggregate(t.aggregate, t.max_aggregate, t.chunks);
      t.inefficiencies = Detect(t.chunks, pattern_params_);
      t.usage_score = UsageScore(t.chunks);
      t.lifetime_score = LifetimeScore(
          t.chunks,
          filter_max_time_ * 0.01f);  // 1 percent lifetime for region threshold
      t.useful_lifetime_score = UsefulLifetimeScore(t.chunks);
      for (auto c : t.chunks) {
        total_alloc_time += c->alloc_call_time;
      }
      t.alloc_time_total = total_alloc_time;
      global_alloc_time_ += total_alloc_time;
      total_alloc_time = 0;
      // build the stack tree
    }

    stack_tree_.SetTraces(traces_);

    CalculatePercentilesChunk(traces_, pattern_params_);
    CalculatePercentilesSize(traces_, pattern_params_);
    // leave this sort order until the user changes
    aggregates_.reserve(num_chunks_ * 2);
  }

  virtual void AggregateAll(vector<TimeValue>& values) {
    // build aggregate structure
    // bin via sampling into times and values arrays
    cout << "aggregating all ..." << endl;
    if (aggregates_.empty())
      Aggregate(aggregates_, max_aggregate_, chunks_, num_chunks_);
    // cout << "done, sampling ..." << endl;
    SampleValues(aggregates_, values);
    // cout << "done" << endl;
  }

  virtual void AggregateTrace(vector<TimeValue>& values, uint32_t trace_index) {
    // build aggregate structure
    // bin via sampling into times and values arrays
    Trace& t = traces_[trace_index];
    // cout << "sampling trace ..." << endl;
    SampleValues(t.aggregate, values);
    // cout << "done, with " << values.size() << " values" << endl;
  }

  virtual uint64_t MaxAggregate() { return max_aggregate_; }
  virtual uint64_t MaxTime() { return max_time_; }
  virtual uint64_t MinTime() { return min_time_; }
  virtual uint64_t FilterMaxTime() { return filter_max_time_; }
  virtual uint64_t FilterMinTime() { return filter_min_time_; }
  virtual uint64_t GlobalAllocTime() { return global_alloc_time_; }

  virtual void SetTraceFilter(const string & str) {
    for (auto& s : trace_filters_) {
      if (s == str) {
        return;
      }
    }
    trace_filters_.push_back(str);
    bool filtered = false;
    for (auto& trace : traces_) {
      if (trace.trace.find(str) == string::npos) {
        trace.filtered = true;
        filtered = true;
      }
    }
    if (filtered) aggregates_.clear();
  }

  virtual void SetTypeFilter(string const& str) {
    cout << "adding type filter:" << str << "|\n";
    for (auto& s : type_filters_)
      if (s == str) return;
    type_filters_.push_back(str);

    for (auto& trace : traces_) {
      trace.type_filtered = true;
      for (auto& s : type_filters_) {
        if (s == trace.type) {
          // cout << "not filtering trace type " << trace.type << " since it
          // matched " << s << endl;
          trace.type_filtered = false;
          break;
        }
      }
    }
    // we assume *something* changed
    aggregates_.clear();
  }

  virtual void TraceFilterReset() {
    if (!trace_filters_.empty()) aggregates_.clear();
    trace_filters_.clear();
    for (auto& trace : traces_) trace.filtered = false;
  }

  virtual void TypeFilterReset() {
    if (!type_filters_.empty()) aggregates_.clear();
    type_filters_.clear();
    for (auto& trace : traces_) trace.type_filtered = false;
  }

  virtual void Traces(vector<TraceValue>& traces) {
    // TODO TraceValue not really needed, could just pass pointers to Trace
    // and convert directly to V8 objects
    TraceValue tmp;
    traces.reserve(traces_.size());
    for (size_t i = 0; i < traces_.size(); i++) {
      if (IsTraceFiltered(traces_[i])) continue;

      tmp.trace = traces_[i].trace;
      tmp.trace_index = i;
      tmp.chunk_index = 0;
      bool overlaps = false;
      for (auto& chunk : traces_[i].chunks) {
        if (chunk->timestamp_start < filter_max_time_ &&
            chunk->timestamp_end > filter_min_time_) {
          overlaps = true;
          break;
        }
      }
      if (!overlaps) continue;

      tmp.num_chunks = traces_[i].chunks.size();
      tmp.type = traces_[i].type;
      tmp.alloc_time_total = traces_[i].alloc_time_total;
      tmp.max_aggregate = traces_[i].max_aggregate;
      tmp.usage_score = traces_[i].usage_score;
      tmp.lifetime_score = traces_[i].lifetime_score;
      tmp.useful_lifetime_score = traces_[i].useful_lifetime_score;
      traces.push_back(tmp);
    }
  }

  virtual void TraceChunks(vector<Chunk*>& chunks, uint32_t trace_index,
                   uint64_t chunk_index, uint64_t num_chunks) {
    // TODO adjust indexing to account for time filtering
    chunks.reserve(num_chunks);
    if (trace_index >= traces_.size()) {
      cout << "TRACE INDEX OVER SIZE\n";
      return;
    }
    Trace& t = traces_[trace_index];
    if (chunk_index >= t.chunks.size()) {
      cout << "CHUNK INDEX OVER SIZE\n";
      return;
    }
    uint64_t bound = chunk_index + num_chunks;
    if (bound > t.chunks.size()) bound = t.chunks.size();
    for (uint64_t i = chunk_index; i < bound; i++) {
      cout << "interval low:" << t.chunks[i]->access_interval_low << "\n";
      chunks.push_back(t.chunks[i]);
    }
  }

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

  virtual uint64_t Inefficiences(uint32_t trace_index) {
    return traces_[trace_index].inefficiencies;
  }

  virtual void StackTreeObject(const v8::FunctionCallbackInfo<v8::Value>& args) {
    stack_tree_.V8Objectify(args);
  }

  virtual void StackTreeAggregate(function<double(const Trace* t)> f) {
    stack_tree_.Aggregate(f);
  }

 private:
  Chunk* chunks_ = nullptr;
  vector<TimeValue> aggregates_;
  uint32_t num_chunks_;
  vector<Trace> traces_;
  char* chunk_ptr_ = nullptr;
  uint64_t min_time_ = UINT64_MAX;
  uint64_t max_time_ = 0;
  uint64_t max_aggregate_ = 0;
  uint64_t filter_max_time_;
  uint64_t filter_min_time_;
  uint64_t global_alloc_time_ = 0;
  PatternParams pattern_params_;
  unordered_multimap<string, pair<string, string>> type_map_;

  StackTree stack_tree_;

  vector<string> trace_filters_;
  vector<string> type_filters_;
  priority_queue<TimeValue> queue_;

  bool InitTypeData(const string& dir_path, string& msg) {
      string dir(dir_path + "typefiles/");
      vector<string> files;
      if (GetFiles(dir, files) != 0) {
          msg = "Directory " + dir +
              " did not contain valid type files for program\n";
          cout << msg << endl;
          return true;
      }
      string line;
      ifstream infile;
      type_map_.clear();
      for (auto& f : files) {
          if (f == "." || f == "..") continue;

          infile.open(dir + f);

          //.Users.byma.projects.bamtools.src.third_party.jsoncpp.json_value.cpp.types
          auto final_dot = f.find_last_of('.');
          auto first_dot = f.find_last_of('.', final_dot - 1);
          first_dot = f.find_last_of('.', first_dot - 1);
          if (first_dot == string::npos) first_dot = 0;
          string module = f.substr(first_dot, final_dot);
          cout << "module is " << module << endl;
          while (getline(infile, line)) {
              auto pos = line.find_first_of("|");
              if (pos == string::npos) {
                  msg = "detected incorrect formatting in line " + line + "\n";
                  return false;
              }
              auto location =
                  line.substr(0, pos);  // equiv to last line of stack trace
              // module is needed to differentiate locations in headers that may be
              // included in multiple files
              auto type_value = make_pair(
                      module,
                      line.substr(pos + 1, line.size() - 1));  // (module, type) pair
              type_map_.insert(make_pair(location, type_value));
              // type_map_[line.substr(0, pos)] = std::make_pair(module,
              // line.substr(pos+1, line.size() - 1));
          }
          infile.close();
      }
      cout << "the types are: \n";
      for (auto& k : type_map_) {
          cout << k.first << " -> "
              << "(" << k.second.first << ", " << k.second.second << ")" << endl;
      }
      return true;
  }

  string Typify(const string& trace) {
    // now i admit, that this is indeed hacky, and entirely
    // dependent on stack traces being produced by llvm-symbolizer
    // or at least ending in dir/filename.cpp:<line>:<col>
    // if/when we switch to symbolizing here, we will have more options
    // and more robust code
    string type;
    auto pos = trace.find_first_of("|");
    auto pos2 = trace.find_first_of("|", pos + 1);
    if (pos == string::npos || pos2 == string::npos)
      return type;

    pos = pos2;
    while (trace[pos] != ' ') pos--;
    string value = trace.substr(pos + 1, pos2 - pos - 1);
    /* cout << "got value:" << value << endl; */
    /* cout << "with mapped types: " << endl; */

    auto range = type_map_.equal_range(value);
    auto position = std::numeric_limits<string::size_type>::max();
    for (auto ty = range.first; ty != range.second; ty++) {
      /* cout << "(" << ty->second.first << ", " << ty->second.second << ")\n"; */

      auto p = trace.find(ty->second.first);
      if (p != string::npos && p < position) {
        position = p;
        type = ty->second.second;
      }
    }

    return type;
  }

  inline bool IsTraceFiltered(Trace const& t) {
    return t.filtered || t.type_filtered;
  }

  int GetFiles(string dir, vector<string>& files) {
    DIR* dp;
    struct dirent* dirp;
    if ((dp = opendir(dir.c_str())) == NULL) {
      cout << "Error(" << errno << ") opening " << dir << endl;
      return errno;
    }

    while ((dirp = readdir(dp)) != NULL) {
      files.push_back(string(dirp->d_name));
    }
    closedir(dp);
    return 0;
  }

  // TODO sampling will miss max values that can be pretty stark sometimes
  // probably need to make sure that particular MAX or MIN values appear in the
  // sample
  void SampleValues(const vector<TimeValue>& points,
                    vector<TimeValue>& values) {
    values.clear();
    values.reserve(MAX_POINTS + 2);
    // first, find the filtered interval
    // cout << "points size is " << points.size() << endl;
    unsigned int j = 0;
    for (; j < points.size() && points[j].time < filter_min_time_; j++)
      ;
    int min = j;
    j++;
    // cout << "min " << min << " j now " << j << endl;
    if (j == points.size()) {
      // basically, there were no points inside the interval,
      // so we set a straight line of the appropriate value during the filter
      // interval
      values.push_back({filter_min_time_, 0});
      values.push_back({filter_min_time_ + 1, points[min - 1].value});
      values.push_back({filter_max_time_ - 1, points[min - 1].value});
      values.push_back({filter_max_time_, 0});
      return;
    }
    for (; j < points.size() && points[j].time <= filter_max_time_; j++)
      ;
    int max = j;
    // cout << "max " << max << endl;
    int num_points = max - min + 1;
    if (num_points == 0 || num_points < 0) {
      cout << "num points is buggy: num points " << num_points << endl;
      return;
    }

    if (num_points > MAX_POINTS) {
      // float bin_size = float(num_points) / float(MAX_POINTS);
      values.push_back(
          {filter_min_time_, points[min == 0 ? min : min - 1].value});
      /*auto sz = points.size();
      for (uint32_t i = 0; i < num_points; i++) {
        // find max value in interval [min + i, min + i + bin_size]
        const TimeValue* maxval = &points[min+i];
        for (auto x = min + i; x < min + i + bin_size && x < sz; x++)
          if (points[x].value > maxval->value) maxval = &points[x];
        values.push_back(*maxval);
        i += bin_size - 1;
      }*/
      // sample MAX POINTS points
      float interval = (float)num_points / (float)MAX_POINTS;
      int i = 0;
      float index = 0.0f;
      while (i < MAX_POINTS) {
        uint64_t idx = (uint64_t)index + min;
        if (idx >= points.size()) {
          cout << "POSSIBLE BUG points size is " << points.size() << " idx is "
               << idx << " interval is " << interval << endl;
          break;
        }
        values.push_back(points[idx]);
        index += interval;
        i++;
      }
    } else {
      values.resize(max - min + 1);
      values[0] = {filter_min_time_, points[min == 0 ? min : min - 1].value};
      memcpy(&values[1], &points[min], sizeof(TimeValue) * (max - min));
    }
    // cout << "first value is " << values[0].value << " at time " <<
    // values[0].time << endl;
    if (values[values.size() - 1].time > filter_max_time_)
      values[values.size() - 1].time = filter_max_time_;
    else
      values.push_back({filter_max_time_ > values[values.size() - 1].time
                            ? filter_max_time_
                            : points[points.size() - 1].time,
                        values[values.size() - 1].value});

    // cout << "values size is " << values.size() << endl;
  }

  void Aggregate(vector<TimeValue>& points, uint64_t& max_aggregate,
                 Chunk* chunks, uint64_t num_chunks) {
    if (!queue_.empty()) {
      cout << "THE QUEUE ISNT EMPTY MAJOR ERROR";
      return;
    }
    TimeValue tmp;
    int64_t running = 0;
    points.clear();
    points.push_back({0, 0});

    uint64_t i = 0;
    while (i < num_chunks) {
      if (IsTraceFiltered(traces_[chunks[i].stack_index])) {
        i++;
        continue;
      }
      if (!queue_.empty() && queue_.top().time < chunks[i].timestamp_start) {
        tmp.time = queue_.top().time;
        running += queue_.top().value;
        tmp.value = running;
        queue_.pop();
        points.push_back(tmp);
      } else {
        running += chunks[i].size;
        if (running > max_aggregate) max_aggregate = running;
        tmp.time = chunks[i].timestamp_start;
        tmp.value = running;
        points.push_back(tmp);
        tmp.time = chunks[i].timestamp_end;
        tmp.value = -chunks[i].size;
        queue_.push(tmp);
        i++;
      }
    }
    // drain the queue
    while (!queue_.empty()) {
      tmp.time = queue_.top().time;
      running += queue_.top().value;
      tmp.value = running;
      queue_.pop();
      points.push_back(tmp);
    }
    // points.push_back({max_time_,0});
  }

  // TODO deduplicate this code
  void Aggregate(vector<TimeValue>& points, uint64_t& max_aggregate,
                 vector<Chunk*>& chunks) {
    int num_chunks = chunks.size();
    if (!queue_.empty()) {
      cout << "THE QUEUE ISNT EMPTY MAJOR ERROR";
      return;
    }
    TimeValue tmp;
    int64_t running = 0;
    points.clear();
    points.push_back({0, 0});

    int i = 0;
    while (i < num_chunks) {
      if (IsTraceFiltered(traces_[chunks[i]->stack_index])) {
        i++;
        continue;
      }
      if (!queue_.empty() && queue_.top().time < chunks[i]->timestamp_start) {
        tmp.time = queue_.top().time;
        running += queue_.top().value;
        tmp.value = running;
        queue_.pop();
        points.push_back(tmp);
      } else {
        running += chunks[i]->size;
        if (running > max_aggregate) max_aggregate = running;
        tmp.time = chunks[i]->timestamp_start;
        tmp.value = running;
        points.push_back(tmp);
        tmp.time = chunks[i]->timestamp_end;
        tmp.value = -chunks[i]->size;
        queue_.push(tmp);
        i++;
      }
    }
    // drain the queue
    while (!queue_.empty()) {
      tmp.time = queue_.top().time;
      running += queue_.top().value;
      tmp.value = running;
      queue_.pop();
      points.push_back(tmp);
    }
  }
};

}  // namespace memoro
