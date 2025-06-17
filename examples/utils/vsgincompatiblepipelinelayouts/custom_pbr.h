#pragma once

#include <vsg/maths/vec3.h>

namespace custom
{
    // OpenGL style fog struct to pass to the GPU
    struct Fog
    {
        vsg::vec3 color = {1.0f, 1.0f, 1.0f};
        float density = 0.0005f; // OpenGL default is 1.0!
        float start = 0.0f;
        float end = 1.0f;
        float exponent = 1.0f;

        void read(vsg::Input& input)
        {
            input.read("color", color);
            input.read("density", density);
            input.read("start", start);
            input.read("end", end);
            input.read("exponent", exponent);
        }

        void write(vsg::Output& output) const
        {
            output.write("color", color);
            output.write("density", density);
            output.write("start", start);
            output.write("end", end);
            output.write("exponent", exponent);
        }
    };

    using FogValue = vsg::Value<Fog>;

    struct Highlight
    {
        vsg::vec3 color = {0.2f, 0.2f, 0.0f};
        bool highlighted = false;

        Highlight() = default;
        Highlight(bool in_highlighted)
            : highlighted(in_highlighted)
        {}

        void read(vsg::Input& input)
        {
            input.read("highlighted", highlighted);
            input.read("color", color);
        }

        void write(vsg::Output& output) const
        {
            output.write("highlighted", highlighted);
            output.write("color", color);
        }
    };

    using HighlightValue = vsg::Value<Highlight>;

    extern vsg::ref_ptr<vsg::ShaderSet> pbr_ShaderSet(vsg::ref_ptr<const vsg::Options> options);
} // namespace custom

template<>
constexpr bool vsg::has_read_write<custom::Fog>()
{
    return true;
}

EVSG_type_name(custom::FogValue);

template<>
constexpr bool vsg::has_read_write<custom::Highlight>()
{
    return true;
}

EVSG_type_name(custom::HighlightValue);
