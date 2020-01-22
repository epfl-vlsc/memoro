#pragma once

#include <cstdint>
#include <istream>

#include <vector>

#include "Memoro.hpp"

class ChunkReader {
  private:
  std::istream &chunkfile_;

  size_t chunk_buf_count_ = 0;
  std::unique_ptr<Chunk[]> chunk_buf_;

  size_t chunk_index_ = 0, chunk_count_ = 0;
  size_t trace_index_ = -1;

  Header header_;

  size_t ReadChunks();

  public:
  ChunkReader(std::istream &chunkfile, size_t chunk_buf_size = 32 * 1024 * 1024 * sizeof(Chunk)) :
    chunkfile_(chunkfile),
    chunk_buf_count_(chunk_buf_size / sizeof(Chunk)),
    chunk_buf_(std::make_unique<Chunk[]>(chunk_buf_count_)),
    chunk_index_(chunk_buf_count_),
    header_(chunkfile) {

    size_t chunk_start = sizeof(struct Header) + header_.index_size * sizeof(uint16_t);
    chunkfile_.seekg(chunk_start);
  }

  const uint32_t Size() { return header_.index_size; }
  const Header& Header() { return header_; }

  std::vector<Chunk> NextTrace();
};
