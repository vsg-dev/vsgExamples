/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "ShaderSet.h"

#include <vsg/io/Options.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>

using namespace vsg;

ShaderSet::ShaderSet()
{
}

ShaderSet::ShaderSet(const ShaderStages& in_stages) : stages(in_stages)
{
}

ShaderSet::~ShaderSet()
{
}

void ShaderSet::addAttributeBinding(std::string name, std::string define, uint32_t location, VkFormat format, ref_ptr<Data> data)
{
    attributeBindings.push_back(AttributeBinding{name, define, location, format, data});
}

void ShaderSet::addUniformBinding(std::string name, std::string define, uint32_t set, uint32_t binding, VkDescriptorType descriptorType, uint32_t descriptorCount, VkShaderStageFlags stageFlags, ref_ptr<Data> data)
{
    uniformBindings.push_back(UniformBinding{name, define, set, binding, descriptorType, descriptorCount, stageFlags, data});
}

const AttributeBinding& ShaderSet::getAttributeBinding(const std::string& name) const
{
    for(auto& binding : attributeBindings)
    {
        if (binding.name == name) return binding;
    }
    return _nullAttributeBinding;
}

const UniformBinding& ShaderSet::getUniformBinding(const std::string& name) const
{
    for(auto& binding : uniformBindings)
    {
        if (binding.name == name) return binding;
    }
    return _nullUniformBinding;
}


ShaderStages ShaderSet::getShaderStages(ref_ptr<ShaderCompileSettings> scs)
{
    if (auto itr = varients.find(scs); itr != varients.end())
    {
        return itr->second;
    }

    auto& new_stages = varients[scs];
    for(auto& stage : stages)
    {
        if (vsg::compare_pointer(stage->module->hints, scs) == 0)
        {
            new_stages.push_back(stage);
        }
        else
        {
            auto new_stage = vsg::ShaderStage::create();
            new_stage->flags = stage->flags;
            new_stage->stage = stage->stage;
            new_stage->module = ShaderModule::create(stage->module->source, scs);
            new_stage->entryPointName = stage->entryPointName;
            new_stage->specializationConstants = stage->specializationConstants;
            new_stages.push_back(new_stage);
        }
    }

    return new_stages;
}

void ShaderSet::read(Input& input)
{
}

void ShaderSet::write(Output& output) const
{
}
