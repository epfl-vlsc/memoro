#include <string>
#include <fstream>

#include "Memoro.hpp"
#include "TraceReader.hpp"
#include "ChunkReader.hpp"
#include "TraceStatWriter.hpp"

using namespace std;

#define STATS_EXT ".stats"

void action_header(char *tracepath, char *chunkpath) {
  FILE *tracefile = fopen(tracepath, "r");
  if (tracefile == nullptr)
    throw "failed to open trace file: "s + tracepath;

  FILE *chunkfile = fopen(chunkpath, "r");
  if (chunkfile == nullptr)
    throw "failed to open chunk file: "s + chunkpath;

  printf("\nHeader for %s\n", tracepath);
  TraceReader tr(tracefile);
  tr.Header().Print();

  printf("\nHeader for %s\n", chunkpath);
  ChunkReader cr(chunkfile);
  cr.Header().Print();

  fclose(chunkfile);
  fclose(tracefile);
}

void action_stats(char *tracepath, char *chunkpath) {
  FILE *tracefile = fopen(tracepath, "r");
  if (tracefile == nullptr)
    throw "failed to open trace file: "s + tracepath;

  FILE *chunkfile = fopen(chunkpath, "r");
  if (chunkfile == nullptr)
    throw "failed to open chunk file: "s + chunkpath;

  TraceReader tr(tracefile);
  ChunkReader cr(chunkfile);

  vector<Trace> traces;
  traces.reserve(tr.Size());

  for (uint32_t i = 0; i < tr.Size(); ++i) {
  /* for (uint32_t i = 0; i < 1; ++i) { */
    Trace trace = tr.Next();
    trace.chunks = cr.NextTrace();

    trace.Stats();
    /* printf("Usage for %u: %g\n", i, trace.usage_score); */
    traces.push_back(trace);
  }

  printf("Chunk size: %lu\n", sizeof(Chunk));

  fclose(chunkfile);
  fclose(tracefile);

  char *outpath = tracepath; // Will overwrite tracepath
  char *outext = strrchr(outpath, '.');
  if (outext == NULL)
    outpath = "out.stats";
  else
    strncpy(outext, STATS_EXT, sizeof(STATS_EXT));

  std::ofstream outfile{ outpath, std::ios::binary };
  TraceStatWriter(traces).Write(outfile);
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf("usage: %s <action> <tracefile> <chunkfile>\n", argv[0]);
    exit(1);
  }

  char *action = argv[1],
       *tracepath = argv[2],
       *chunkpath = argv[3];

  action_header(tracepath, chunkpath);

  if (strcmp(action, "header") == 0)
    action_header(tracepath, chunkpath);
  if (strcmp(action, "stats") == 0)
    action_stats(tracepath, chunkpath);
  else {
    printf("Unknown action: %s", action);
    exit(2);
  }
}
