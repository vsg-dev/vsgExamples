#pragma once

#include <vsg/utils/Instrumentation.h>
#include <vsg/ui/FrameStamp.h>
#include <vsg/io/stream.h>

namespace vsg
{
    class ProfileLog : public Inherit<Object, ProfileLog>
    {
    public:

        ProfileLog(size_t size = 16384);

        enum Type : uint8_t
        {
            NO_TYPE,
            FRAME,
            CPU,
            COMMAND_BUFFER,
            GPU
        };

        struct Entry
        {
            Type type = {};
            bool enter = true;
            vsg::time_point time = {};
            const SourceLocation* sourceLocation = nullptr;
            const Object* object = nullptr;
            uint64_t reference = 0;
        };

        std::vector<Entry> entries;
        std::atomic_uint64_t index = 0;
        std::vector<uint64_t> frameIndices;

        Entry& enter(uint64_t& reference, Type type)
        {
            reference = index.fetch_add(1);
            Entry& enter_entry = entry(reference);
            enter_entry.enter = true;
            enter_entry.type = type;
            enter_entry.reference = 0;
            enter_entry.time = clock::now();
            return enter_entry;
        }

        Entry& leave(uint64_t& reference, Type type)
        {
            Entry& enter_entry = entry(reference);

            uint64_t new_reference = index.fetch_add(1);
            Entry& leave_entry = entry(new_reference);

            enter_entry.reference = new_reference;
            leave_entry.time = clock::now();
            leave_entry.enter = false;
            leave_entry.type = type;
            leave_entry.reference = reference;
            reference = new_reference;
            return leave_entry;
        }

        Entry& entry(uint64_t reference)
        {
            return entries[reference % entries.size()];
        }

        void report(std::ostream& out);
        void report(std::ostream& out, uint64_t reference);

    public:
        void read(Input& input) override;
        void write(Output& output) const override;
    };
    VSG_type_name(ProfileLog)

    class Profiler : public Inherit<Instrumentation, Profiler>
    {
    public:

        Profiler();

        struct Settings : public Inherit<Object, Settings>
        {
            unsigned int cpu_instrumentation_level = 1;
            unsigned int gpu_instrumentation_level = 1;
        };

        ref_ptr<Settings> settings;
        mutable ref_ptr<ProfileLog> log;

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
    VSG_type_name(Profiler)
}
