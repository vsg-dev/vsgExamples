#include <vsg/all.h>

namespace vsg
{
    class GraphicsPipelineBuilder : public Inherit<Object, GraphicsPipelineBuilder>
    {
    public:

        class Traits : public Inherit<Object, Traits>
        {
        public:
            ShaderStages shaderStages;

            using BindingFormat = std::vector<VkFormat>;
            using BindingFormats = std::vector<BindingFormat>;

            std::map<VkVertexInputRate, BindingFormats> vertexAttributes;

            using BindingTypes = std::vector<VkDescriptorType>;
            using BindingSet = std::map<VkShaderStageFlags, BindingTypes>;

            std::vector<BindingSet> descriptorLayouts;

            ColorBlendState::ColorBlendAttachments colorBlendAttachments;
            VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        };

        GraphicsPipelineBuilder(Paths searchPaths, Allocator* allocator = nullptr);
        
        virtual void build(vsg::ref_ptr<Traits> traits);

        ref_ptr<GraphicsPipeline> getGraphicsPipeline() const { return _graphicsPipeline; }

        static size_t sizeOf(VkFormat format);
        static size_t sizeOf(const std::vector<VkFormat>& format);

    protected:
        ref_ptr<GraphicsPipeline> _graphicsPipeline;
    };
    VSG_type_name(GraphicsPipelineBuilder)

    class TextGraphicsPipelineBuilder : public Inherit<GraphicsPipelineBuilder, TextGraphicsPipelineBuilder>
    {
    public:
        TextGraphicsPipelineBuilder(Paths searchPaths, Allocator* allocator = nullptr);
    protected:
        
    };
    VSG_type_name(TextGraphicsPipelineBuilder)

    //
    // Font state group that binds atlas and lookup texture descriptors
    //
    class Font : public Inherit<BindDescriptorSets, Font>
    {
    public:
        Font(PipelineLayout* pipelineLayout, const std::string& fontname, Paths searchPaths);

        struct GlyphData
        {
            uint16_t character;
            vec4 uvrect; // min x/y, max x/y
            vec2 size; // normalised size of the glyph
            vec2 offset; // normalised offset
            float xadvance; // normalised xadvance
            float lookupOffset; // offset into lookup texture
        };
        using GlyphMap = std::map<uint16_t, GlyphData>;

        GlyphMap& getGlyphMap() { return _glyphs; }
        const GlyphMap& getGlyphMap() const { return _glyphs; }

        float getHeight() const { return _fontHeight; }
        float getNormalisedLineHeight() const { return _normalisedLineHeight; }

    protected:

        // data
        GlyphMap _glyphs;

        float _fontHeight; // height font was exported at in pixels
        float _normalisedLineHeight; // line height normailsed against fontHeight

        // descriptors
        ref_ptr<DescriptorImage> _atlasTexture;
        ref_ptr<DescriptorImage> _glyphUVsTexture; // vec4 uvrect (x,y,width,height)
        ref_ptr<DescriptorImage> _glyphSizesTexture; // vec4 size, offset, (offsetx,offsety, sizex,sizey)
    };
    VSG_type_name(Font)

    //
    // TextMetrics
    //
    struct TextMetrics
    {
        float height;
        float lineHeight;
        vec3 billboardAxis;
    };


    //
    // TextMetricsValue
    //
    class VSG_DECLSPEC TextMetricsValue : public Inherit<Value<TextMetrics>, TextMetricsValue>
    {
    public:
        TextMetricsValue() {}

        void read(Input& input) override
        {
        }
        void write(Output& output) const override
        {
        }
    };
    VSG_type_name(TextMetricsValue)


    //
    // GlyphInstanceData
    //
    struct GlyphInstanceData {
        vec3 position; // 3d offset position for text
        vec2 offset; // 2d offset within paragraph/text block
        float lookupOffset; // offset/coord into font lookup textures
    };
    VSG_array(GlyphInstanceDataArray, GlyphInstanceData);

    //
    // GlyphGeometry
    //
    class GlyphGeometry : public Inherit<Command, GlyphGeometry>
    {
    public:
        GlyphGeometry(Allocator* allocator = nullptr);

        void compile(Context& context) override;
        void dispatch(CommandBuffer& commandBuffer) const override;

        // settings
        ref_ptr<Data> _glyphInstances;

        using Commands = std::vector<ref_ptr<Command>>;

        // compiled object
        Commands _renderImplementation;
    };
    VSG_type_name(GlyphGeometry)

    //
    // TextBase
    //
    class TextBase : public Inherit<StateGroup, TextBase>
    {
    public:
        TextBase(Font* font, GraphicsPipeline* pipeline, Allocator* allocator = nullptr);

        Font* getFont() const { return _font; }
        void setFont(Font* font);

        const float getFontHeight() const { return _textMetrics->value().height; }
        void setFontHeight(const float& fontHeight);

        const vec3 getBillboardAxis() const { return _textMetrics->value().billboardAxis; }
        void setBillboardAxis(const vec3& billboardAxis);

        void setPosition(const vec3& position);

        void buildTextGraph();

    protected:
        virtual ref_ptr<GlyphGeometry> createInstancedGlyphs() = 0;

        // data
        ref_ptr<Font> _font;
        ref_ptr<TextMetricsValue> _textMetrics;

        // graph objects
        ref_ptr<DescriptorBuffer> _textMetricsUniform;
        ref_ptr<MatrixTransform> _transform;
    };
    VSG_type_name(TextBase)

    //
    // Text
    //
    class Text : public Inherit<TextBase, Text>
    {
    public:
        Text(Font* font, GraphicsPipeline* pipeline, Allocator* allocator = nullptr);

        const std::string& getText() const { return _text; }
        void setText(const std::string& text);

    protected:
        ref_ptr<GlyphGeometry> createInstancedGlyphs() override;

        // data
        std::string _text;
    };
    VSG_type_name(Text)

      
    //
    // TextGroup
    //
    class TextGroup : public Inherit<TextBase, TextGroup>
    {
    public:
        TextGroup(Font* font, GraphicsPipeline* pipeline, Allocator* allocator = nullptr);

        void addText(const std::string& text, const vec3& position);
        const uint32_t getNumTexts() { return static_cast<uint32_t>(_texts.size()); }
        void clear();

    protected:
        ref_ptr<GlyphGeometry> createInstancedGlyphs() override;
        
        std::vector<std::string> _texts;
        std::vector<vec3> _positions;
    };
    VSG_type_name(TextGroup)

    // loads glyph data info from a unity3d font file
    extern bool readUnity3dFontMetaFile(const std::string& filePath, Font::GlyphMap& glyphMap, float& fontPixelHeight, float& normalisedLineHeight);

}
