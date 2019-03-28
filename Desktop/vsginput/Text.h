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

        ref_ptr<Texture> _texture;

        struct GlyphData
        {
            uint16_t character;
            vec4 uvrect; // min x/y, max x/y
            vec2 size; // normalised size of the glyph
            vec2 offset; // normalised offset
            float xadvance; // normalised xadvance 
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

    class Text : public Inherit<StateGroup, Text>
    {
    public:
        Text(Font* font, TextGroup* group, Allocator* allocator = nullptr);

        const std::string& getText() const { return _text; }
        void setText(const std::string& text);

        Font* getFont() const { return _font; }
        void setFont(Font* font) { _font = font; }

        const float getFontHeight() const { return _fontHeight; }
        void setFont(const float& fontHeight) { _fontHeight = fontHeight; }

        void setPosition(const vec3& position);

        ref_ptr<Geometry> createGlyphQuad(const vec3& corner, const vec3& widthVec, const vec3& heightVec, float l, float b, float r, float t);

    protected:
        ref_ptr<Font> _font;
        ref_ptr<MatrixTransform> _transform;
        std::string _text;
        float _fontHeight;
    };
    VSG_type_name(Text)

}
