#pragma once

#include <cstdint>
#include <ostream>
#include <iostream>

#include <vector>

#include "Memoro.hpp"

class TraceChunkWriter {
  private:
  std::ostream &tracefile_;
  std::ostream &chunkfile_;

  void WriteTraces(const std::vector<Trace> &traces) {
    Header header(traces.size());
    tracefile_.write((char*)&header, sizeof(Header));

    for (const auto& t: traces) {
      uint16_t trace_size = (uint16_t)t.trace.size();
      tracefile_.write((char*)&trace_size, sizeof(uint16_t));
    }

    for (const auto& t: traces)
      tracefile_ << t.trace;
  }

  void WriteChunks(const std::vector<Trace> &traces) {
    uint32_t index_size = 0;
    for (const auto& t: traces)
      index_size += t.chunks.size();

    Header header(index_size);
    chunkfile_.write((char*)&header, sizeof(header));

    uint16_t chunk_size = sizeof(Chunk);
    for (const auto& t: traces)
      for (const auto& c: t.chunks)
        chunkfile_.write((char*)&chunk_size, sizeof(uint16_t));

    for (const auto& t: traces) {
      std::cout << "Writting " << t.chunks.size() << " chunks at offset " << chunkfile_.tellp() << std::endl;
      chunkfile_.write((char*)t.chunks.data(), t.chunks.size() * sizeof(Chunk));
    }
  }

  public:
  TraceChunkWriter(std::ostream &tracefile, std::ostream &chunkfile) :
    tracefile_(tracefile), chunkfile_(chunkfile) {}

  void Write(const std::vector<Trace> &traces) {
    WriteTraces(traces);
    WriteChunks(traces);
  }
};
