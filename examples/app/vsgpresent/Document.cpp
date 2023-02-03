#include "Document.h"

using namespace Markdown;

void DocumentVisitor::apply(Item&) {}

void DocumentVisitor::apply(Image& image) { apply(static_cast<Item&>(image)); }

void DocumentVisitor::apply(Link& link) { apply(static_cast<Item&>(link)); }

void DocumentVisitor::apply(Text& text) { apply(static_cast<Item&>(text)); }

void DocumentVisitor::apply(Paragraph& paragraph) { apply(static_cast<Item&>(paragraph)); }

void DocumentVisitor::apply(Heading& heading) { apply(static_cast<Item&>(heading)); }

void DocumentVisitor::apply(HorizontalRule& horizontalRule) { apply(static_cast<Item&>(horizontalRule)); }

void DocumentVisitor::apply(OrderedList& orderedList) { apply(static_cast<Item&>(orderedList)); }

void DocumentVisitor::apply(UnorderedList& unorderedList) { apply(static_cast<Item&>(unorderedList)); }

void DocumentVisitor::apply(Document& document) { apply(static_cast<Item&>(document)); }
