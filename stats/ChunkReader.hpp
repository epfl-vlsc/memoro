#pragma once

#include <stdio.h>
#include <stdint.h>

#include <vector>

#include "Memoro.hpp"

class ChunkReader {
  private:
  FILE *chunkfile_;

  size_t chunk_buf_count_;
  Chunk *chunk_buf_;

  size_t chunk_index_, chunk_count_;
  size_t trace_index_ = -1;

  Header header_;

  size_t ReadChunks();

  public:
  ChunkReader(FILE *chunkfile, size_t chunk_buf_size = 32 * 1024 * 1024 * sizeof(Chunk)) :
    chunkfile_(chunkfile),
    chunk_buf_count_(chunk_buf_size / sizeof(Chunk)),
    chunk_buf_(new Chunk[chunk_buf_count_]),
    chunk_index_(chunk_buf_count_),
    header_(chunkfile) {
    if (chunk_buf_ == nullptr)
      throw "Unable to alloc chunk_buf_";

    size_t chunk_start = sizeof(struct Header) + header_.index_size * sizeof(uint16_t);
    fseek(chunkfile, chunk_start, SEEK_SET);
  }

  const uint32_t Size() { return header_.index_size; }
  const Header& Header() { return header_; }

  std::vector<Chunk> NextTrace();
};
