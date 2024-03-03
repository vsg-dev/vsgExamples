
#include "Profiler.h"

#include <vsg/ui/FrameStamp.h>
#include <vsg/io/Logger.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProfileLog
//
ProfileLog::ProfileLog(size_t size) :
    entries(size)
{
}

void ProfileLog::read(Input& input)
{
    entries.resize(input.readValue<uint64_t>("entries"));
}

void ProfileLog::write(Output& output) const
{
    output.writeValue<uint64_t>("entries", entries.size());
}

void ProfileLog::report(std::ostream& out)
{
    indentation indent;
    out<<"ProfileLog::report() entries.size() = "<<entries.size()<<std::endl;
    out<<"{"<<std::endl;
    uint32_t tab = 2;
    indent += tab;

    for(uint64_t i=0; i<index.load(); ++i)
    {
        auto& entry = entries[i];
        out<<indent<<"{ "<<entry.type;
        if (entry.sourceLocation) out<<", file="<<entry.sourceLocation->file<<", func="<<entry.sourceLocation->function<<", line="<<entry.sourceLocation->line;
        if (entry.object) out<<", "<<entry.object->className();
        out << " }"<<std::endl;
    }

    indent -= tab;
    out<<"}"<<std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Profiler
//
Profiler::Profiler() :
    settings(Profiler::Settings::create()),
    log(ProfileLog::create())
{
}

void Profiler::setThreadName(const std::string& name) const
{
    vsg::info("Profiler::setThreadName(", name, ")");
}

void Profiler::enterFrame(const SourceLocation* sl, uint64_t& reference, FrameStamp& frameStamp) const
{
    if (settings->cpu_instrumentation_level >= sl->level || settings->gpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->enter(reference);
        entry.type = ProfileLog::FRAME_ENTER;
        entry.sourceLocation = sl;
        entry.object = &frameStamp;
    }
}

void Profiler::leaveFrame(const SourceLocation* sl, uint64_t& reference, FrameStamp& frameStamp) const
{
    if (settings->cpu_instrumentation_level >= sl->level || settings->gpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->leave(reference);
        entry.type = ProfileLog::FRAME_LEAVE;
        entry.sourceLocation = sl;
        entry.object = &frameStamp;
    }
}

void Profiler::enter(const SourceLocation* sl, uint64_t& reference, const Object* object) const
{
    if (settings->cpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->enter(reference);
        entry.type = ProfileLog::CPU_ENTER;
        entry.sourceLocation = sl;
        entry.object = object;
    }
}

void Profiler::leave(const SourceLocation* sl, uint64_t& reference, const Object* object) const
{
    if (settings->cpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->leave(reference);
        entry.type = ProfileLog::CPU_LEAVE;
        entry.sourceLocation = sl;
        entry.object = object;
    }
}

void Profiler::enterCommandBuffer(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer) const
{
    if (settings->gpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->enter(reference);
        entry.type = ProfileLog::COMMAND_BUFFER_ENTER;
        entry.sourceLocation = sl;
        entry.object = &commandBuffer;
    }
}

void Profiler::leaveCommandBuffer(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer) const
{
    if (settings->gpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->leave(reference);
        entry.type = ProfileLog::COMMAND_BUFFER_LEAVE;
        entry.sourceLocation = sl;
        entry.object = &commandBuffer;
    }
}

void Profiler::enter(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer, const Object* object) const
{
    if (settings->gpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->enter(reference);
        entry.type = ProfileLog::GPU_ENTER;
        entry.sourceLocation = sl;
        entry.object = object;
    }
}

void Profiler::leave(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer, const Object* object) const
{
    if (settings->gpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->leave(reference);
        entry.type = ProfileLog::GPU_LEAVE;
        entry.sourceLocation = sl;
        entry.object = object;
    }
}
