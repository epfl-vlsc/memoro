#include <cstdio>
#include <vector>

#include "ChunkReader.hpp"

using namespace std;

size_t ChunkReader::ReadChunks() {
  chunkfile_.read((char*)chunk_buf_.get(), chunk_buf_count_ * sizeof(Chunk));
  chunk_count_ = chunkfile_.gcount() / sizeof(Chunk);
  chunk_index_ = 0;

  /* printf("Asked for %lux%lu, got %lu chunks\n", chunk_buf_count_, sizeof(Chunk), chunk_count_); */

  return chunk_count_;
}

vector<Chunk> ChunkReader::NextTrace() {
  vector<Chunk> chunks;
  chunks.reserve(1024);

  ++trace_index_;

  do {
    // Refill the buffer if needed
    if (chunk_index_ >= chunk_count_) {
      if (ReadChunks() == 0)
        break;
    }

    // Get one chunk out of the buffer
    Chunk &chunk = chunk_buf_[chunk_index_];
    /* printf("TI: %lu, CTI: %u, CI: %lu, %p\n", trace_index_, chunk.stack_index, chunk_index_, &chunk); */
    if (chunk.stack_index != trace_index_)
      break;

    // Add it to the chunks array
    chunks.push_back(chunk);
    ++chunk_index_;
  } while (true);

  chunks.shrink_to_fit();
  /* printf("Trace %lu has %lu chunks!\n", trace_index_, chunks.size()); */

  return chunks;
}
