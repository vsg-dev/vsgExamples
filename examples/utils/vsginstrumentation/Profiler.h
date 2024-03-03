#pragma once

#include <vsg/utils/Instrumentation.h>
#include <vsg/io/stream.h>

namespace vsg
{
    class Profiler : public Inherit<Instrumentation, Profiler>
    {
    public:

        Profiler();

        struct Settings : public Inherit<Object, Settings>
        {
            unsigned int cpu_instrumentation_level = 0;
            unsigned int gpu_instrumentation_level = 0;
        };

        ref_ptr<Settings> settings;
        mutable std::atomic_uint64_t index = 0;
        mutable indentation indent;

    public:
        virtual void setThreadName(const std::string& /*name*/) const;

        virtual void enterFrame(const SourceLocation* /*sl*/, uint64_t& /*reference*/, FrameStamp& /*frameStamp*/) const;
        virtual void leaveFrame(const SourceLocation* /*sl*/, uint64_t& /*reference*/, FrameStamp& /*frameStamp*/) const;

        virtual void enter(const SourceLocation* /*sl*/, uint64_t& /*reference*/, const Object* /*object*/ = nullptr) const;
        virtual void leave(const SourceLocation* /*sl*/, uint64_t& /*reference*/, const Object* /*object*/ = nullptr) const;

        virtual void enterCommandBuffer(const SourceLocation* /*sl*/, uint64_t& /*reference*/, CommandBuffer& /*commandBuffer*/) const;
        virtual void leaveCommandBuffer(const SourceLocation* /*sl*/, uint64_t& /*reference*/, CommandBuffer& /*commandBuffer*/) const;

        virtual void enter(const SourceLocation* /*sl*/, uint64_t& /*reference*/, CommandBuffer& /*commandBuffer*/, const Object* /*object*/ = nullptr) const;
        virtual void leave(const SourceLocation* /*sl*/, uint64_t& /*reference*/, CommandBuffer& /*commandBuffer*/, const Object* /*object*/ = nullptr) const;
    };
}
