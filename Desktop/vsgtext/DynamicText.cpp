/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Array2D.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>
#include <vsg/state/DescriptorImage.h>

#include "DynamicText.h"
#include "GpuLayoutTechnique.h"

#include <iostream>

using namespace vsg;

FontState::FontState(Font* font)
{
    {
        auto sampler = Sampler::create();
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler->borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        sampler->anisotropyEnable = VK_TRUE;
        sampler->maxAnisotropy = 16.0f;
        sampler->magFilter = VK_FILTER_LINEAR;
        sampler->minFilter = VK_FILTER_LINEAR;
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler->maxLod = 12.0;

        textureAtlas = DescriptorImage::create(sampler, font->atlas, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }

    {
        auto sampler = Sampler::create();
        sampler->magFilter = VK_FILTER_NEAREST;
        sampler->minFilter = VK_FILTER_NEAREST;
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler->unnormalizedCoordinates = VK_TRUE;

        auto glyphMetricsProxy = vec4Array2D::create(font->glyphMetrics, 0, sizeof(GlyphMetrics), sizeof(GlyphMetrics)/sizeof(vec4), font->glyphMetrics->valueCount(), Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT});
        glyphMetrics = DescriptorImage::create(sampler, glyphMetricsProxy, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }
}

void DynamicText::read(Input& input)
{
    Node::read(input);

    input.readObject("font", font);
    input.readObject("technique", technique);
    input.readObject("layout", layout);
    input.readObject("text", text);

    setup();
}

void DynamicText::write(Output& output) const
{
    Node::write(output);

    output.writeObject("font", font);
    output.writeObject("technique", technique);
    output.writeObject("layout", layout);
    output.writeObject("text", text);
}

void DynamicText::setup(uint32_t minimumAllocation)
{
    if (!layout) layout = LeftAlignment::create();
    if (!technique) technique = GpuLayoutTechnique::create();

    technique->setup(this, minimumAllocation);
}
