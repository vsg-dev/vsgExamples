#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/nodes/Node.h>
#include <vsg/state/StateGroup.h>
#include <vsg/state/DescriptorBuffer.h>
#include <vsg/text/Font.h>
#include <vsg/text/TextLayout.h>

namespace vsg
{

    struct LayoutStruct
    {
        vec3 position = vec3(0.0f, 0.0f, 0.0f); float pad0;
        vec3 horizontal = vec3(1.0f, 0.0f, 0.0f); float pad1;
        vec3 vertical = vec3(0.0f, 1.0f, 0.0f); float pad2;
        vec4 color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        vec4 outlineColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
        float outlineWidth = 0.0f;

        void read(vsg::Input& input)
        {
            input.read("position", position);
            input.read("horizontal", horizontal);
            input.read("vertical", vertical);
            input.read("color", color);
            input.read("outlineColor", outlineColor);
            input.read("outlineWidth", outlineWidth);
        }

        void write(vsg::Output& output) const
        {
            output.write("position", position);
            output.write("horizontal", horizontal);
            output.write("vertical", vertical);
            output.write("color", color);
            output.write("outlineColor", outlineColor);
            output.write("outlineWidth", outlineWidth);
        }
    };

    template<>
    constexpr bool has_read_write<LayoutStruct>() { return true; }

    VSG_value(TextLayoutValue, LayoutStruct);

    class VSG_DECLSPEC DynamicText : public Inherit<Node, DynamicText>
    {
    public:
        template<class N, class V>
        static void t_traverse(N& node, V& visitor)
        {
            if (node.renderingBackend) node.renderingBackend->stategroup->accept(visitor);
        }

        void traverse(Visitor& visitor) override { t_traverse(*this, visitor); }
        void traverse(ConstVisitor& visitor) const override { t_traverse(*this, visitor); }
        void traverse(RecordTraversal& visitor) const override { t_traverse(*this, visitor); }

        void read(Input& input) override;
        void write(Output& output) const override;

        /// settings
        ref_ptr<Font> font;
        ref_ptr<TextLayout> layout;
        ref_ptr<Data> text;

        /// create the rendering backend.
        /// minimumAllocation provides a hint for the minimum number of glyphs to allocate space for.
        virtual void setup(uint32_t minimumAllocation = 0);

        /// Wraooer for Font::textureAtlas data.
        struct VSG_DECLSPEC FontState : public Inherit<Object, FontState>
        {
            FontState(Font* font);
            bool match() const { return true; }

            ref_ptr<DescriptorImage> textureAtlas;
            ref_ptr<DescriptorImage> glyphMetrics;
        };


        /// rendering state used to set up grahics pipeline and descriptor sets, assigned to Font to allow it be be shared
        struct VSG_DECLSPEC RenderingState : public Inherit<Object, RenderingState>
        {
            RenderingState(Font* font);

            bool match() const { return true; }

            ref_ptr<PipelineLayout> pipelineLayout;
            ref_ptr<DescriptorSetLayout> textArrayDescriptorSetLayout;
            ref_ptr<BindGraphicsPipeline> bindGraphicsPipeline;
            ref_ptr<BindDescriptorSet> bindDescriptorSet;
        };

        /// rendering backend container holds all the scene graph elements required to render the text, filled in by DynamicText::setup().
        struct RenderingBackend : public Inherit<Object, RenderingBackend>
        {
            ref_ptr<vec3Array> vertices;
            ref_ptr<Data> indices;
            ref_ptr<DrawIndexed> drawIndexed;

            ref_ptr<uintArray> textArray;
            ref_ptr<TextLayoutValue> layoutValue;
            ref_ptr<DescriptorBuffer> textDescriptor;
            ref_ptr<DescriptorBuffer> layoutDescriptor;
            ref_ptr<BindDescriptorSet> bindTextDescriptorSet;

            ref_ptr<BindVertexBuffers> bindVertexBuffers;
            ref_ptr<BindIndexBuffer> bindIndexBuffer;
            ref_ptr<RenderingState> sharedRenderingState;
            ref_ptr<StateGroup> stategroup;
        };

        ref_ptr<RenderingBackend> renderingBackend;

    protected:
    };
    VSG_type_name(vsg::DynamicText);

} // namespace vsg
