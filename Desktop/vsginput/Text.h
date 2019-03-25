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
            uint32_t x; 
            uint32_t y;
            uint32_t width;
            uint32_t height;
            uint32_t xoffset;
            uint32_t yoffset;
            uint32_t xadvance;

            vec2 uvOrigin;
            vec2 uvSize;
        };
        using GlyphMap = std::map<uint16_t, GlyphData>;

        GlyphMap _glyphs;

        uint32_t _lineHeight;
        uint32_t _base;
        uint32_t _scaleWidth;
        uint32_t _scaleHeight;
    };
    VSG_type_name(Font)

    class Text : public Inherit<StateGroup, Text>
    {
    public:
        Text(Font* font, TextGroup* group, Allocator* allocator = nullptr);

        void setText(const std::string& text);

        ref_ptr<Geometry> createGlyphQuad(const vec3& corner, const vec3& widthVec, const vec3& heightVec, float l, float b, float r, float t);

        ref_ptr<Font> _font;
        std::string _text;
    };
    VSG_type_name(Text)

}
