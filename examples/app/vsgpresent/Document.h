#include <vsg/core/Inherit.h>

namespace Markdown
{
    class Item;
    class Image;
    class Link;
    class Text;
    class Paragraph;
    class Heading;
    class HorizontalRule;
    class OrderedList;
    class UnorderedList;
    class Document;

    /// Implement a new base visitor class to add support for the various Markdown classes
    struct DocumentVisitor : public vsg::Visitor
    {
        virtual void apply(Item&);
        virtual void apply(Image& image);
        virtual void apply(Link& link);
        virtual void apply(Text& text);
        virtual void apply(Paragraph& paragraph);
        virtual void apply(Heading& heading);
        virtual void apply(HorizontalRule& horizontalRule);
        virtual void apply(OrderedList& orderedList);
        virtual void apply(UnorderedList& unorderedList);
        virtual void apply(Document& document);
    };

    struct Item : public vsg::Inherit<vsg::Object, Item>
    {
        virtual void accept(DocumentVisitor& visitor) { visitor.apply(*this); }
    };

    struct Image : public vsg::Inherit<Item, Image>
    {
        Image(const std::string_view& in_sv) : filename(in_sv) {}
        void accept(DocumentVisitor& visitor) override { visitor.apply(*this); }

        std::string filename;
    };

    struct Link : public vsg::Inherit<Item, Link>
    {
        Link(const std::string_view& in_sv) : url(in_sv) {}
        void accept(DocumentVisitor& visitor) override { visitor.apply(*this); }

        std::string url;
    };

    struct Text : public vsg::Inherit<Item, Text>
    {
        Text(const std::string_view& in_sv) : text(in_sv) {}
        void accept(DocumentVisitor& visitor) override { visitor.apply(*this); }

        std::string text;
    };

    struct Heading : public vsg::Inherit<Item, Heading>
    {
        Heading(size_t in_size, const std::string_view& in_sv) : size(in_size), text(in_sv) {}
        void accept(DocumentVisitor& visitor) override { visitor.apply(*this); }

        size_t size = 1;
        std::string text;
    };

    struct HorizontalRule : public vsg::Inherit<Item, HorizontalRule>
    {
        void accept(DocumentVisitor& visitor) override { visitor.apply(*this); }
    };

    struct OrderedList : public vsg::Inherit<Item, OrderedList>
    {
        void accept(DocumentVisitor& visitor) override { visitor.apply(*this); }
        void add(vsg::ref_ptr<Item> item) { items.push_back(item); }

        std::list<vsg::ref_ptr<Item>> items;
    };

    struct UnorderedList : public vsg::Inherit<Item, UnorderedList>
    {
        void accept(DocumentVisitor& visitor) override { visitor.apply(*this); }
        void add(vsg::ref_ptr<Item> item) { items.push_back(item); }

        std::list<vsg::ref_ptr<Item>> items;
    };

    struct Paragraph : public vsg::Inherit<Item, Paragraph>
    {
        Paragraph(const std::string_view& in_sv) { items.push_back(Text::create(in_sv)); }
        void accept(DocumentVisitor& visitor) override { visitor.apply(*this); }
        void add(vsg::ref_ptr<Item> item) { items.push_back(item); }

        std::list<vsg::ref_ptr<Item>> items;
    };

    struct Document : public vsg::Inherit<Item, Document>
    {
        void accept(DocumentVisitor& visitor) override { visitor.apply(*this); }
        void add(vsg::ref_ptr<Item> item) { items.push_back(item); }

        std::list<vsg::ref_ptr<Item>> items;
    };

};

EVSG_type_name(Markdown::Item)
EVSG_type_name(Markdown::Image)
EVSG_type_name(Markdown::Link)
EVSG_type_name(Markdown::Text)
EVSG_type_name(Markdown::Paragraph)
EVSG_type_name(Markdown::Heading)
EVSG_type_name(Markdown::HorizontalRule)
EVSG_type_name(Markdown::OrderedList)
EVSG_type_name(Markdown::UnorderedList)
EVSG_type_name(Markdown::Document)
