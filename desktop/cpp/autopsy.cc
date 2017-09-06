
#include "autopsy.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <vector>

using namespace std;

struct __attribute__((packed)) Header {
  uint8_t version_major = 0;
  uint8_t version_minor = 1;
  uint8_t compression_type = 0;
  uint16_t segment_start;
  uint32_t index_size;
};

class Dataset {
  public:
    Dataset() {}
    void Reset(string& dir_path) {
      string trace_file = dir_path + "hplgst.trace";
      cout << "file path is " << trace_file << endl;
      // fopen trace file, build traces array
      FILE* trace_fd = fopen(trace_file.c_str(), "r");
      if (trace_fd == NULL) {
        cout << "failed to open file\n";
        return;
      }
      Header header;
      fread(&header, sizeof(Header), 1, trace_fd);
      cout << "trace header version " << (int)header.version_major << "." << 
        (int)header.version_minor << " with index size " << header.index_size << endl;

      vector<uint16_t> index;
      index.resize(header.index_size);
      fread(&index[0], 2, header.index_size, trace_fd);

      traces.reserve(header.index_size);
      Trace t;
      char trace_buf[4096];
      for (int i = 0; i < header.index_size; i++) {
        if (index[i] > 4096)
          cout << "index is too big!! " << index[i] << endl;
        else {
          fread(trace_buf, index[i], 1, trace_fd);
          t.trace = string(trace_buf, index[i]);
          cout << "got trace: " << t.trace << endl;
          traces.push_back(t);
        }
      }
      cout << " got " << traces.size() << " traces" << endl;
      fclose(trace_fd);

      string chunk_file = dir_path + "hplgst.chunks";
      // for some reason I can't mmap the file so we open and copy ...
      FILE* chunk_fd = fopen(chunk_file.c_str(), "r");
      int file_size = 0;
      if (chunk_fd == NULL) {
        cout << "Failed to open chunk file" << endl;
        return;
      } else {
        cout << "getting file size\n";
        fseek(chunk_fd, 0L, SEEK_END);
        file_size = ftell(chunk_fd);
        rewind(chunk_fd);
      }
     
      cout << "file size " << file_size << endl;
      fread(&header, sizeof(Header), 1, chunk_fd);
      cout << "chunk header version " << (int)header.version_major << "." << 
        (int)header.version_minor << " with index size " << header.index_size << endl;
      
      index.resize(header.index_size);
      fread(&index[0], 2, header.index_size, chunk_fd);

      num_chunks = header.index_size;
      file_size = file_size - sizeof(Header) - header.index_size*2;
      cout << "file size " << file_size << endl;
      chunk_ptr_ = new char[file_size];
      fread(chunk_ptr_, file_size, 1, chunk_fd);

      chunks = (Chunk*) (chunk_ptr_);
      for (int i = 0; i < num_chunks; i++) {
        cout << i << " : parent: " << chunks[i].stack_index << " size: " << chunks[i].size << ", start: " <<
          chunks[i].timestamp_start << endl;
      }
    }

  private:
    Chunk* chunks;
    uint32_t num_chunks;
    vector<Trace> traces;
    char* chunk_ptr_;


};

static Dataset theDataset;

void SetDataset(std::string& file_path) {
  theDataset.Reset(file_path);
  cout << "returning \n" ;
  return;

}

