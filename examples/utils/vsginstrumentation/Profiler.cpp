
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
    out<<"frames "<<frameIndices.size()<<std::endl;
    for(auto frameIndex : frameIndices)
    {
        report(out, frameIndex);
        out<<std::endl;
    }
}

void ProfileLog::report(std::ostream& out, uint64_t reference)
{
    indentation indent;
    out<<"ProfileLog::report("<<reference<<")"<<std::endl;
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

    uint64_t startReference = reference;
    uint64_t endReference = entry(reference).reference;

    if (startReference > endReference)
    {
        std::swap(startReference, endReference);
    }

    for(uint64_t i = startReference; i <= endReference; ++i)
    {
        auto& first = entry(i);
        auto& second = entry(first.reference);

        auto duration = std::abs(std::chrono::duration<double, std::chrono::milliseconds::period>(second.time - first.time).count());

        if (first.enter) out<<indent<<"{ ";
        else
        {
            indent -= tab;
            out<<indent<<"} ";
        }

        out<<typeNames[first.type]<<", duration = "<<duration<<"ms, ";

        if (first.sourceLocation) out/*<<", file="<<first.sourceLocation->file*/<<", func="<<first.sourceLocation->function<<", line="<<first.sourceLocation->line;
        // if (first.object) out<<", "<<first.object->className();

        out << std::endl;

        if (first.enter) indent += tab;
    }
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
        uint64_t startReference = reference;
        auto& entry = log->leave(reference, ProfileLog::FRAME);
        entry.sourceLocation = sl;
        entry.object = &frameStamp;
        uint64_t endReference = reference;

        if (endReference >= static_cast<uint64_t>(log->entries.size()))
        {
            uint32_t safeReference = endReference - log->entries.size();
            size_t i = 0;
            for(; i<log->frameIndices.size(); ++i)
            {
                if (log->frameIndices[i] > safeReference)
                {
                    break;
                }
            }
            if (i > 0)
            {
                log->frameIndices.erase(log->frameIndices.begin(), log->frameIndices.begin() + i);
            }
        }

        log->frameIndices.push_back(startReference);
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
