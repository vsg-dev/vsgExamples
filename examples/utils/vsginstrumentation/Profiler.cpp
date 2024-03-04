
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
    uint32_t tab = 1;
    indent += tab;

    const char* typeNames[] = {
        "NO_TYPE",
        "FRAME",
        "CPU",
        "COMMAND_BUFFER",
        "GPU"
    };

    for(uint64_t i=0; i<index.load(); ++i)
    {
        auto& entry = entries[i];
        auto& paired = entries[entry.reference];

        auto duration = std::abs(std::chrono::duration<double, std::chrono::milliseconds::period>(entry.time - paired.time).count());

        if (entry.type == FRAME && entry.enter) out<<std::endl;

        if (entry.enter) out<<indent<<"{ ";
        else
        {
            indent -= tab;
            out<<indent<<"} ";
        }

        out<<typeNames[entry.type]<<", duration = "<<duration<<"ms, ";

        if (entry.sourceLocation) out/*<<", file="<<entry.sourceLocation->file*/<<", func="<<entry.sourceLocation->function<<", line="<<entry.sourceLocation->line;
        // if (entry.object) out<<", "<<entry.object->className();

        out << std::endl;

        if (entry.enter) indent += tab;
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
        auto& entry = log->enter(reference, ProfileLog::FRAME);
        entry.sourceLocation = sl;
        entry.object = &frameStamp;
    }
}

void Profiler::leaveFrame(const SourceLocation* sl, uint64_t& reference, FrameStamp& frameStamp) const
{
    if (settings->cpu_instrumentation_level >= sl->level || settings->gpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->leave(reference, ProfileLog::FRAME);
        entry.sourceLocation = sl;
        entry.object = &frameStamp;
    }
}

void Profiler::enter(const SourceLocation* sl, uint64_t& reference, const Object* object) const
{
    if (settings->cpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->enter(reference, ProfileLog::CPU);
        entry.sourceLocation = sl;
        entry.object = object;
    }
}

void Profiler::leave(const SourceLocation* sl, uint64_t& reference, const Object* object) const
{
    if (settings->cpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->leave(reference, ProfileLog::CPU);
        entry.sourceLocation = sl;
        entry.object = object;
    }
}

void Profiler::enterCommandBuffer(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer) const
{
    if (settings->gpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->enter(reference, ProfileLog::COMMAND_BUFFER);
        entry.sourceLocation = sl;
        entry.object = &commandBuffer;
    }
}

void Profiler::leaveCommandBuffer(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer) const
{
    if (settings->gpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->leave(reference, ProfileLog::COMMAND_BUFFER);
        entry.sourceLocation = sl;
        entry.object = &commandBuffer;
    }
}

void Profiler::enter(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer, const Object* object) const
{
    if (settings->gpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->enter(reference, ProfileLog::GPU);
        entry.sourceLocation = sl;
        entry.object = object;
    }
}

void Profiler::leave(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer, const Object* object) const
{
    if (settings->gpu_instrumentation_level >= sl->level)
    {
        auto& entry = log->leave(reference, ProfileLog::GPU);
        entry.sourceLocation = sl;
        entry.object = object;
    }
}
