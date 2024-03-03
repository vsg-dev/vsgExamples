
#include "Profiler.h"

#include <vsg/ui/FrameStamp.h>
#include <vsg/io/Logger.h>

using namespace vsg;

void Profiler::setThreadName(const std::string& name) const
{
    vsg::info("Profiler::setThreadName(", name, ")");
}

void Profiler::enterFrame(const SourceLocation* sl, uint64_t& reference, FrameStamp& frameStamp) const
{
    reference = index.fetch_add(1);
    vsg::info("\n\nProfiler::enterFrame(", sl, ", ", reference, ", ", frameStamp.frameCount, ")");
}

void Profiler::leaveFrame(const SourceLocation* sl, uint64_t& reference, FrameStamp& frameStamp) const
{
    vsg::info("Profiler::leaveFrame(", sl, ", ", reference, ", ", frameStamp.frameCount, ")");
}


void Profiler::enter(const SourceLocation* sl, uint64_t& reference, const Object* object) const
{
    reference = index.fetch_add(1);
    if (object) vsg::info("Profiler::enter(", sl, ", ", reference, ", ", object->className(), ")");
    else vsg::info("Profiler::enter(", sl, ", ", reference, ", nullptr )");
}

void Profiler::leave(const SourceLocation* sl, uint64_t& reference, const Object* object) const
{
    if (object) vsg::info("Profiler::leave(", sl, ", ", reference, ", ", object->className(), ")");
    else vsg::info("Profiler::leave(", sl, ", ", reference, ", nullptr )");
}


void Profiler::enterCommandBuffer(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer) const
{
    reference = index.fetch_add(1);
    vsg::info("Profiler::enterCommandBuffer(", sl, ", ", reference, ", ", &commandBuffer, ")");
}

void Profiler::leaveCommandBuffer(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer) const
{
    vsg::info("Profiler::leaveCommandBuffer(", sl, ", ", reference, ", ", &commandBuffer, ")");
}

void Profiler::enter(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer, const Object* object) const
{
    if (object) vsg::info("Profiler::enter CB(", sl, ", ", reference, ", ", &commandBuffer, ", ", object->className(), ")");
    else vsg::info("Profiler::enter CB(", sl, ", ", reference, ", nullptr )");
}

void Profiler::leave(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer, const Object* object) const
{
    if (object) vsg::info("Profiler::leave CB(", sl, ", ", reference, ", ", &commandBuffer, ", ", object->className(), ")");
    else vsg::info("Profiler::leave CB(", sl, ", ", reference, ", nullptr )");
}

