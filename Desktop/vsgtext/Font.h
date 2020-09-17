#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Data.h>
#include <vsg/io/Options.h>
#include <vsg/state/GraphicsPipeline.h>
#include <vsg/state/DescriptorSet.h>

namespace vsg
{
    class Font : public vsg::Inherit<vsg::Object, Font>
    {
    public:

        struct GlyphData
        {
            uint16_t character;
            vsg::vec4 uvrect; // min x/y, max x/y
            vsg::vec2 size; // normalised size of the glyph
            vsg::vec2 offset; // normalised offset
            float xadvance; // normalised xadvance
            float lookupOffset; // offset into lookup texture
        };
        using GlyphMap = std::map<uint16_t, GlyphData>;

        vsg::ref_ptr<vsg::Data> atlas;
        vsg::ref_ptr<vsg::DescriptorImage> texture;
        GlyphMap glyphs;
        float fontHeight;
        float normalisedLineHeight;
        vsg::ref_ptr<vsg::Options> options;

        /// Technique base class provide ability to provide range of different rendering techniques
        class Technique : public vsg::Inherit<vsg::Object, Technique>
        {
        public:
            vsg::ref_ptr<vsg::BindGraphicsPipeline> bindGraphicsPipeline;
            vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
        };

        std::vector<vsg::ref_ptr<Technique>> techniques;

        /// get or create a Technique instance that matches the specified type
        template<class T>
        vsg::ref_ptr<T> getTechnique()
        {
            for(auto& technique : techniques)
            {
                auto tech = technique.cast<T>();
                if (tech) return tech;
            }
            auto tech = T::create(this);
            techniques.emplace_back(tech);
            return tech;
        }
    };
}
