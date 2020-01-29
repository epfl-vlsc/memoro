#include <string>
#include <fstream>

#include "Memoro.hpp"
#include "TraceReader.hpp"
#include "ChunkReader.hpp"
#include "TraceStatWriter.hpp"
#include "TraceChunkWriter.hpp"

using namespace std;

#define STATS_EXT ".stats"

string basename(const string& fullname) {
  auto extpos = fullname.rfind('.');
  return fullname.substr(0, extpos);
}

void action_help(int argc, char *argv[]) {
  printf("usage: %s <action> ...\n", argv[0]);
  printf("action:\n");
  printf("  header <tracefile> <chunkfile>\n");
  printf("  stats  <tracefile> <chunkfile>\n");
  printf("  filter <tracefile> <chunkfile> <traceneedle>\n");
}

void action_header(int argc, char *argv[]) {
  if (argc != 2) {
    printf("error: wrong number of argument!\n");
    action_help(argc, argv);
    exit(2);
  }

  string tracepath{ argv[0] };
  string chunkpath{ argv[1] };

  ifstream tracefile{ tracepath, ios::binary };
  if (!tracefile)
    throw "failed to open trace file: "s + tracepath;

  ifstream chunkfile{ chunkpath, ios::binary };
  if (!chunkfile)
    throw "failed to open chunk file: "s + chunkpath;

  printf("\nHeader for %s\n", tracepath.c_str());
  Header{ tracefile }.Print();

  printf("\nHeader for %s\n", chunkpath.c_str());
  Header{ chunkfile }.Print();
}

void action_stats(int argc, char *argv[]) {
  if (argc != 2) {
    printf("error: wrong number of argument!\n");
    action_help(argc, argv);
    exit(2);
  }

  string tracepath{ argv[0] };
  string chunkpath{ argv[1] };

  action_header(argc, argv);

  ifstream tracefile{ tracepath, ios::binary };
  if (!tracefile)
    throw "failed to open trace file: "s + tracepath;

  ifstream chunkfile{ chunkpath, ios::binary };
  if (!chunkfile)
    throw "failed to open chunk file: "s + chunkpath;

  TraceReader tr(tracefile);
  ChunkReader cr(chunkfile);

  vector<Trace> traces;
  traces.reserve(tr.Size());

  for (uint32_t i = 0; i < tr.Size(); ++i) {
    Trace trace = tr.Next();
    trace.chunks = cr.NextTrace();

    trace.Stats();
    /* printf("Usage for %u: %g\n", i, trace.usage_score); */
    traces.push_back(trace);
  }

  printf("Chunk size: %lu\n", sizeof(Chunk));

  string outpath = basename(tracepath) + ".stats";
  std::ofstream outfile{ outpath, std::ios::binary };
  TraceStatWriter(traces).Write(outfile);
}

void action_filter(int argc, char *argv[]) {
  if (argc != 3) {
    printf("error: wrong number of argument!\n");
    action_help(argc, argv);
    exit(2);
  }

  string tracepath{ argv[0] };
  string chunkpath{ argv[1] };
  string needle{ argv[2] };

  ifstream tracefile{ tracepath, ios::binary };
  if (!tracefile)
    throw "failed to open trace file: "s + tracepath;

  ifstream chunkfile{ chunkpath, ios::binary };
  if (!chunkfile)
    throw "failed to open chunk file: "s + chunkpath;

  TraceReader tr(tracefile);
  ChunkReader cr(chunkfile);

  vector<Trace> traces;
  for (uint32_t i = 0; i < tr.Size(); ++i) {
    Trace trace = tr.Next();
    trace.chunks = cr.NextTrace();

    if (trace.trace.find(needle) == string::npos)
      continue;

    printf("Adding a new Trace: %lu\n", traces.size());

    for (auto &chunk: trace.chunks)
      chunk.stack_index = traces.size();

    traces.push_back(trace);
  }

  printf("Traces found: %lu\n", traces.size());

  string basepath = basename(tracepath);

  string out_tracepath = basepath + ".filter.trace";
  ofstream out_tracefile{ out_tracepath, ios::binary };
  if (!out_tracefile)
    throw "failed to open out trace file: "s + out_tracepath;

  string out_chunkpath = basepath + ".filter.chunks";
  ofstream out_chunkfile{ out_chunkpath, ios::binary };
  if (!out_chunkfile)
    throw "failed to open out chunk file: "s + out_chunkpath;

  TraceChunkWriter(out_tracefile, out_chunkfile)
    .Write(traces);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    action_help(argc, argv);
    exit(1);
  }

  string action{ argv[1] };

  argc -= 2;
  argv += 2;

  if (action == "help")
    action_help(argc, argv);
  else if (action == "header")
    action_header(argc, argv);
  else if (action == "stats")
    action_stats(argc, argv);
  else if (action == "filter")
    action_filter(argc, argv);
  else {
    printf("unknown action: %s\n", action.c_str());
    action_help(argc, argv);
    exit(2);
  }
}
