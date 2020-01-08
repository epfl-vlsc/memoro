#pragma once

#include <stdio.h>
#include <stdint.h>

#include "Memoro.hpp"

class TraceReader {
  private:
    FILE *tracefile_;
    Header header_;

    std::vector<uint16_t> index_;

    uint32_t trace_index_{0};

  public:
    TraceReader(FILE *tracefile) : tracefile_(tracefile), header_(tracefile), index_(header_.index_size) {
      index_.reserve(header_.index_size);
      fread(index_.data(), sizeof(uint16_t), header_.index_size, tracefile_);
    }

    const Header& Header() const { return header_; }
    const uint32_t Size() const { return header_.index_size; }

    Trace Next();
};
