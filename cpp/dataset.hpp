#pragma once

#include <string>
#include <vector>

#include "memoro.h"
#include "stacktree.h"

namespace memoro {

class Dataset {
    public:
        virtual void AggregateAll(std::vector<TimeValue>& values) = 0;
        virtual void AggregateTrace(std::vector<TimeValue>& values, uint32_t trace_index) = 0;

        virtual uint64_t MaxAggregate() = 0;
        virtual uint64_t MaxTime() = 0;
        virtual uint64_t MinTime() = 0;
        virtual uint64_t FilterMaxTime() = 0;
        virtual uint64_t FilterMinTime() = 0;
        virtual uint64_t GlobalAllocTime() = 0;

        virtual void SetTraceFilter(const std::string& str) = 0;
        virtual void SetTypeFilter(const std::string& str) = 0;
        virtual void TraceFilterReset() = 0;
        virtual void TypeFilterReset() = 0;

        virtual void Traces(std::vector<TraceValue>& traces) = 0;
        virtual void TraceChunks(std::vector<Chunk*>& chunks, uint32_t trace_index, uint64_t chunk_index, uint64_t num_chunks) = 0;

        virtual void SetFilterMinMax(uint64_t min, uint64_t max) = 0;
        virtual void FilterMinMaxReset() = 0;

        virtual void GlobalInfo(GlobalInfo& gi);
        virtual uint64_t Inefficiences(uint32_t trace_index) = 0;

        virtual void StackTreeObject(const v8::FunctionCallbackInfo<v8::Value>& args) = 0;
        virtual void StackTreeAggregate(std::function<double(const Trace* t)> f) = 0;
        virtual void StackTreeAggregate(const std::string &key) = 0;

        virtual ~Dataset() = 0;
};

} // namespace memoro
