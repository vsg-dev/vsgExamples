#include <vsg/all.h>

namespace vsg
{
    class TextGroup : public Inherit<StateGroup, TextGroup>
    {
    public:
        TextGroup(Paths searchPaths, Allocator* allocator = nullptr);

        ref_ptr<BindGraphicsPipeline> _bindGraphicsPipeline;
    };
    VSG_type_name(TextGroup)

    class Font : public Inherit<Object, Font>
    {
    public:
        Font(const std::string& fontname, Paths searchPaths, Allocator* allocator = nullptr);

        ref_ptr<Texture> _atlasTexture;
        ref_ptr<Texture> _glyphUVsTexture; // vec4 uvrect (x,y,width,height)
        ref_ptr<Texture> _glyphSizesTexture; // vec4 size, offset, (sizex,sizey,offsetx,offsety)

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

        GlyphMap _glyphs;

        float _fontHeight; // height font was exported at in pixels
        float _lineHeight; // line height in pixels
        float _normalisedLineHeight; // line height normailsed against fontHeight
        float _baseLine; // base line
        float _normalisedBaseLine; // base line normailsed againt font height
        float _scaleWidth; // the pixel size of the atlas used to scale to UV space
        float _scaleHeight;
    };
    VSG_type_name(Font)

    struct TextMetrics
    {
        float height;
        float lineHeight;
    };

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

    struct GlyphInstanceData {
        vec2 offset;
        float lookupOffset;
    };
    VSG_array(GlyphInstanceDataArray, GlyphInstanceData);

    class Text : public Inherit<StateGroup, Text>
    {
    public:
        Text(Font* font, TextGroup* group, Allocator* allocator = nullptr);

        const std::string& getText() const { return _text; }
        void setText(const std::string& text);

        Font* getFont() const { return _font; }
        void setFont(Font* font) { _font = font; }

        const float getFontHeight() const { return _fontHeight; }
        void setFontHeight(const float& fontHeight);

        void setPosition(const vec3& position);

    protected:
        ref_ptr<Geometry> createInstancedGlyphs(const std::string& text);

        // data
        ref_ptr<Font> _font;
        std::string _text;
        float _fontHeight;

        // graph objects
        ref_ptr<Uniform> _textMetricsUniform;
        ref_ptr<TextMetricsValue> _textMetrics;
        ref_ptr<MatrixTransform> _transform;
    };
    VSG_type_name(Text)

}
