
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
    vsg::info("\n\n", indent, "Profiler::enterFrame(", sl, ", ", reference, ", ", frameStamp.frameCount, ")");
    indent += 1;
}

void Profiler::leaveFrame(const SourceLocation* sl, uint64_t& reference, FrameStamp& frameStamp) const
{
    indent -= 1;
    vsg::info(indent, "Profiler::leaveFrame(", sl, ", ", reference, ", ", frameStamp.frameCount, ")");
}


void Profiler::enter(const SourceLocation* sl, uint64_t& reference, const Object* object) const
{
    reference = index.fetch_add(1);
    if (object) vsg::info(indent, "Profiler::enter(", sl, ", ", reference, ", ", object->className(), ")");
    else vsg::info(indent, "Profiler::enter(", sl, ", ", reference, ", nullptr )");
    indent += 1;
}

void Profiler::leave(const SourceLocation* sl, uint64_t& reference, const Object* object) const
{
    indent -= 1;
    if (object) vsg::info(indent, "Profiler::leave(", sl, ", ", reference, ", ", object->className(), ")");
    else vsg::info(indent, "Profiler::leave(", sl, ", ", reference, ", nullptr )");
}


void Profiler::enterCommandBuffer(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer) const
{
    reference = index.fetch_add(1);
    vsg::info(indent, "Profiler::enterCommandBuffer(", sl, ", ", reference, ", ", &commandBuffer, ")");
    indent += 1;
}

void Profiler::leaveCommandBuffer(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer) const
{
    indent -= 1;
    vsg::info(indent, "Profiler::leaveCommandBuffer(", sl, ", ", reference, ", ", &commandBuffer, ")");
}

void Profiler::enter(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer, const Object* object) const
{
    if (object) vsg::info(indent, "Profiler::enter CB(", sl, ", ", reference, ", ", &commandBuffer, ", ", object->className(), ")");
    else vsg::info(indent, "Profiler::enter CB(", sl, ", ", reference, ", nullptr )");
    indent += 1;
}

void Profiler::leave(const SourceLocation* sl, uint64_t& reference, CommandBuffer& commandBuffer, const Object* object) const
{
    indent -= 1;
    if (object) vsg::info(indent, "Profiler::leave CB(", sl, ", ", reference, ", ", &commandBuffer, ", ", object->className(), ")");
    else vsg::info(indent, "Profiler::leave CB(", sl, ", ", reference, ", nullptr )");
}

