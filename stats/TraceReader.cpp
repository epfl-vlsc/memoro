#include "TraceReader.hpp"

using namespace std;

Trace TraceReader::Next() {
  if (trace_index_ == header_.index_size)
    throw "Out of Trace"s;

  uint16_t trace_size = index_[trace_index_++];
  char trace_buf[trace_size];

  tracefile_.read((char*)trace_buf, trace_size);

  return Trace{string{trace_buf, trace_size}};
}
