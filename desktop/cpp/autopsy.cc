
#include "autopsy.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <queue>

using namespace std;

#define MAX_POINTS 1500

struct __attribute__((packed)) Header {
  uint8_t version_major = 0;
  uint8_t version_minor = 1;
  uint8_t compression_type = 0;
  uint16_t segment_start;
  uint32_t index_size;
};

struct Trace {
  std::string trace;
  bool filtered = false;
  std::vector<Chunk*> chunks;
};
    

bool operator<(const TimeValue& a, const TimeValue& b) {
  return a.time > b.time;
}

class Dataset {
  public:
    Dataset() {}
    void Reset(string& dir_path) {
      if (chunk_ptr_)
        delete [] chunk_ptr_;
      traces.clear();
      
      string trace_file = dir_path + "hplgst.trace";
      // fopen trace file, build traces array
      FILE* trace_fd = fopen(trace_file.c_str(), "r");
      if (trace_fd == NULL) {
        cout << "failed to open file\n";
        return;
      }
      Header header;
      fread(&header, sizeof(Header), 1, trace_fd);

      vector<uint16_t> index;
      index.resize(header.index_size);
      fread(&index[0], 2, header.index_size, trace_fd);

      traces.reserve(header.index_size);
      Trace t;
      char trace_buf[4096];
      for (int i = 0; i < header.index_size; i++) {
        if (index[i] > 4096) {
          cout << "index is too big!! " << index[i] << endl;
          return;
        } else {
          fread(trace_buf, index[i], 1, trace_fd);
          t.trace = string(trace_buf, index[i]);
          traces.push_back(t);
        }
      }
      fclose(trace_fd);

      string chunk_file = dir_path + "hplgst.chunks";
      // for some reason I can't mmap the file so we open and copy ...
      FILE* chunk_fd = fopen(chunk_file.c_str(), "r");
      int file_size = 0;
      if (chunk_fd == NULL) {
        cout << "Failed to open chunk file" << endl;
        return;
      } else {
        fseek(chunk_fd, 0L, SEEK_END);
        file_size = ftell(chunk_fd);
        rewind(chunk_fd);
      }
     
      fread(&header, sizeof(Header), 1, chunk_fd);
      
      index.resize(header.index_size);
      fread(&index[0], 2, header.index_size, chunk_fd);

      num_chunks = header.index_size;
      file_size = file_size - sizeof(Header) - header.index_size*2;
      chunk_ptr_ = new char[file_size];
      fread(chunk_ptr_, file_size, 1, chunk_fd);

      chunks = (Chunk*) (chunk_ptr_);
      fclose(chunk_fd);

      // sort the chunks makes bin/aggregate easier
      sort(chunks, chunks+num_chunks, [](Chunk& a, Chunk& b) {
            return a.timestamp_start < b.timestamp_start;
          });

      // set trace structure pointers to their chunks
      for (int i = 0; i < num_chunks; i++) {
        Trace& t = traces[chunks[i].stack_index];
        t.chunks.push_back(&chunks[i]);
        if (chunks[i].timestamp_start < min_time)
          min_time = chunks[i].timestamp_start;
        if (chunks[i].timestamp_end > max_time)
          max_time = chunks[i].timestamp_end;
      }
      int i = 0;
      for (auto& t : traces) {
        cout << "trace id " << i << ", " << t.chunks.size() << " chunks. " << endl;
        for (auto c : t.chunks) {
          cout << "chunk size " << c->size << " start: " << c->timestamp_start << endl;
        }
        i++;
      }
      aggregates.reserve(num_chunks*2);
    }

    void AggregateAll(vector<TimeValue>& values) {
      // build aggregate structure
      // bin via sampling into times and values arrays
      priority_queue<TimeValue> queue;
      TimeValue tmp;
      int64_t running = 0;
      aggregates.clear();
      aggregates.push_back({0,0});

      int i = 0;
      while (i < num_chunks) {
        if (traces[chunks[i].stack_index].filtered) {
          i++;
          continue;
        }
        if (!queue.empty() && queue.top().time < chunks[i].timestamp_start) {
          tmp.time = queue.top().time;
          running += queue.top().value;
          tmp.value = running;
          queue.pop();
          aggregates.push_back(tmp);
        } else {
          running += chunks[i].size;
          if (running > max_aggregate)
            max_aggregate = running;
          tmp.time = chunks[i].timestamp_start;
          tmp.value = running;
          aggregates.push_back(tmp);
          tmp.time = chunks[i].timestamp_end;
          tmp.value = -chunks[i].size;
          queue.push(tmp);
          i++;
        }
      }
      // drain the queue
      while (!queue.empty()) {
          tmp.time = queue.top().time;
          running += queue.top().value;
          tmp.value = running;
          queue.pop();
          aggregates.push_back(tmp);
      }
      if (aggregates.size() > MAX_POINTS) {
        // sample MAX POINTS points
        float interval = (float)aggregates.size() / (float)MAX_POINTS;
        int i = 0;
        float index = 0.0f;
        while (i < MAX_POINTS) {
          int idx = (int) index;
          if (idx >= aggregates.size()) {
            cout << " OH FUCK";
            break;
          }
          values.push_back(aggregates[idx]);
          index += interval;
          i++;
        }
      } else {
        values = vector<TimeValue>(aggregates);
      }
    }

    uint64_t MaxAggregate() { return max_aggregate; }
    uint64_t MaxTime() { return max_time; }
    uint64_t MinTime() { return min_time; }

    uint32_t SetTraceFilter(string& str) {
      for (auto& s : trace_filters) 
        if (s == str)
          return;
      trace_filters.push_back(str);
      uint32_t num_traces = 0;
      for (auto& trace : traces) {
        if (trace.trace.find(str) == string::npos)
          trace.filtered = true;
      }
    }

  private:

    Chunk* chunks;
    vector<TimeValue> aggregates;
    uint32_t num_chunks;
    uint32_t num_active_traces;
    vector<Trace> traces;
    char* chunk_ptr_ = nullptr;
    uint64_t min_time = UINT64_MAX;
    uint64_t max_time = 0;
    uint64_t max_aggregate = 0;

    vector<string> trace_filters;
};

// its just easier this way ...
static Dataset theDataset;

void SetDataset(std::string& file_path) {
  theDataset.Reset(file_path);
}

void AggregateAll(std::vector<TimeValue>& values) {
  theDataset.AggregateAll(values);
}

uint64_t MaxAggregate() {
  return theDataset.MaxAggregate();
}

uint64_t MaxTime() {
  return theDataset.MaxTime();
}

uint64_t MinTime() {
  return theDataset.MinTime();
}

void SetTraceKeyword(std::string& keyword) {
  // will only include traces that contain this keyword
  theDataset.SetTraceFilter(keyword);
}



