#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */


#include <vsg/core/compare.h>
#include <vsg/state/ShaderStage.h>

namespace vsg
{

    struct AttributeBinding
    {
        std::string name;
        std::string define;
        uint32_t location = 0;
        VkFormat foramt = VK_FORMAT_UNDEFINED;
    };

    struct UniformBinding
    {
        std::string name;
        std::string define;
        uint32_t set = 0;
        uint32_t              binding = 0;
        VkDescriptorType      descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        uint32_t              descriptorCount = 0;
        VkShaderStageFlags    stageFlags = 0;
    };

    class /*VSG_DECLSPEC*/ ShaderSet : public Inherit<Object, ShaderSet>
    {
    public:

        ShaderSet();
        ShaderSet(const ShaderStages& in_stages);

        /// base ShaderStages that other varients as based on.
        ShaderStages stages;

        std::vector<AttributeBinding> attributeBindings;
        std::vector<UniformBinding> uniformBindings;

        /// varients of the rootShaderModule compiled for differen combinations of ShaderCompileSettings
        std::map<ref_ptr<ShaderCompileSettings>, ShaderStages, DerefenceLess> varients;

        /// get the ShaderStages varient that uses specified ShaderCompileSettings.
        ShaderStages getShaderStages(ref_ptr<ShaderCompileSettings> scs = {});

        void read(Input& input) override;
        void write(Output& output) const override;

    protected:
        virtual ~ShaderSet();
    };
}
