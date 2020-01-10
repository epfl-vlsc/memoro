#pragma once

#include <cstdint>
#include <istream>

#include "Memoro.hpp"

class TraceReader {
  private:
    std::istream &tracefile_;
    Header header_;

    std::vector<uint16_t> index_;

    uint32_t trace_index_{0};

  public:
    TraceReader(std::istream &tracefile) : tracefile_(tracefile), header_(tracefile), index_(header_.index_size) {
      index_.reserve(header_.index_size);
      tracefile_.read((char*)index_.data(), header_.index_size * sizeof(uint16_t));
    }

    const Header& Header() const { return header_; }
    const uint32_t Size() const { return header_.index_size; }

    Trace Next();
};
