#pragma once

#include <vsg/maths/vec3.h>

namespace custom
{
    // OpenGL style fog struct to pass to the GPU
    struct Fog
    {
        vsg::vec3 color = {1.0, 1.0, 1.0};
        float density = 0.05; // OpenGL default is 1.0!
        float start = 0.0;
        float end = 1.0;
        float exponent = 1.0;

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

    extern vsg::ref_ptr<vsg::ShaderSet> pbr_ShaderSet(vsg::ref_ptr<const vsg::Options> options);
}

template<>
constexpr bool vsg::has_read_write<custom::Fog>() { return true; }

EVSG_type_name(custom::FogValue);
