#include <vsg/all.h>
#include <vsgXchange/all.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include "Document.h"

vsg::ref_ptr<vsg::Node> parse_md(const std::string& text)
{
    if (text.empty()) return {};

    auto get_line_and_advance = [](const std::string& str, std::string::size_type& position) -> std::string_view
    {
        if (position >= str.size()) return {};

        auto start_of_line = position;
        auto end_of_line = str.find_first_of("\r\n", position);
        if (end_of_line == std::string::npos)
        {
            position = end_of_line;
            return std::string_view(&str[start_of_line], str.size() - start_of_line);
        }

        if (str[end_of_line] == '\r')
        {
            if ((end_of_line+1) < str.size() && str[end_of_line+1] == '\n') position = (end_of_line+2); // windows line ending
            else position = (end_of_line+1); // mac liine ending
        }
        else position = (end_of_line+1); // unix line ending

        return std::string_view(&str[start_of_line], end_of_line - start_of_line);
    };

    auto remove_white_space = [](std::string_view& sv) -> void
    {
        if (sv.empty()) return;

        std::string_view::size_type count = sv.find_first_not_of(" \t");
        sv.remove_prefix(count);

        if (sv.empty()) return;

        count = sv.find_last_not_of(" \t");
        sv.remove_suffix(sv.size()-1-count);
    };

    auto count_matches = [](std::string_view& sv, char c) -> std::string_view::size_type
    {
        std::string_view::size_type count = 0;
        for(auto itr = sv.begin(); itr != sv.end(); ++itr)
        {
            if ((*itr)==c) ++count;
        }
        return count;
    };

    auto document = Markdown::Document::create();

    std::string::size_type pos = 0;
    while (pos < text.size())
    {
        std::string_view line = get_line_and_advance(text, pos);
        remove_white_space(line);

        if (line.empty())
        {
            continue;
        }

        // check for # Heading
        if (line[0]=='#')
        {
            std::string_view::size_type count = line.find_first_not_of('#');
            if (count != std::string_view::npos && line[count] == ' ')
            {
                line.remove_prefix(count);
                remove_white_space(line);

                // create a header label
                document->add(Markdown::Heading::create(count, line));
                continue;
            }
        }

        // checked for underlined version of Heading
        if (count_matches(line, '=')==line.size())
        {
            // TODO get previously added item, if it's a Text then remove it and add content to currently line
            document->add(Markdown::Heading::create(1, line));
            continue;
        }
        if (count_matches(line, '-')==line.size())
        {
            // TODO get previously added item, if it's a Text then remove it and add content to currently line
            document->add(Markdown::Heading::create(2, line));
            continue;
        }

        if (line[0]=='*' && line.size()>1 && line[1]==' ')
        {
            // check for unorder list.
            line.remove_prefix(2);
            remove_white_space(line);
            // TODO if previous item was an unorderedList then use it
            auto unorderedList = Markdown::UnorderedList::create();
            unorderedList->add(Markdown::Paragraph::create(line));
            document->add(unorderedList);
            continue;
        }

        if ((line[0]>='0' && line[0]<='9') && line.size()>=2)
        {
            // check for order list.
            auto point_pos = line.find_first_of('.', 1);
            if (point_pos != std::string_view::npos)
            {
                auto number_of_digits = static_cast<std::string_view::size_type>(std::count_if(line.begin(), line.begin()+point_pos, [](char c) { return c >= '0' && c <= '9'; }));
                if (number_of_digits == point_pos)
                {
                    if ((point_pos+1) == line.size() || (line[point_pos+1]==' ' || line[point_pos+1]=='\t'))
                    {
                        line.remove_prefix(point_pos+1);
                        remove_white_space(line);

                        // TODO if previous item was an orderedList then use it
                        auto orderedList = Markdown::OrderedList::create();
                        orderedList->add(Markdown::Paragraph::create(line));
                        document->add(orderedList);
                        continue;
                    }
                }
            }
        }

        if (line.size() >= 3 && (line[0]=='-' || line[0]=='*' || line[0]=='_'))
        {
            // check for horizontal rule
            if (count_matches(line, line[0])==line.size())
            {
                document->add(Markdown::HorizontalRule::create());
                continue;
            }
        }

        document->add(Markdown::Paragraph::create(line));
    }

    struct PrintDocuemt : public Markdown::DocumentVisitor
    {
        void apply(Markdown::Item& item) override
        {
            std::cout<<"item = "<<item.className()<<std::endl;
        }

        void apply(Markdown::Text& text) override
        {
            std::cout<<"text = "<<text.text<<std::endl;
        }

        void apply(Markdown::Paragraph& paragraph) override
        {
            std::cout<<"paragraph = ";
            for(auto& item : paragraph.items)
            {
                item->accept(*this);
            }
            std::cout<<std::endl;
        }

        void apply(Markdown::Heading& heading) override
        {
            std::cout<<"heading = "<<heading.size<<", "<<heading.text<<std::endl;
        }

        void apply(Markdown::OrderedList& orderedList) override
        {
            std::cout<<"orderedList = "<<std::endl;
            size_t count = 1;
            for(auto& item : orderedList.items)
            {
                std::cout<<count<<". ";
                item->accept(*this);
                ++count;
            }
        }

        void apply(Markdown::UnorderedList& unorderedList) override
        {
            std::cout<<"unorderedList = "<<std::endl;
            for(auto& item : unorderedList.items)
            {
                std::cout<<"  * ";
                item->accept(*this);
            }
        }

        void apply(Markdown::Document& document) override
        {
            std::cout<<"doc = "<<document.className()<<std::endl;
            for(auto& item : document.items)
            {
                item->accept(*this);
            }
        }
    };

    std::cout<<"Document read "<<document<<std::endl;

    vsg::visit<PrintDocuemt>(document);

    return {};
}

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // set up vsg::Options to pass in filepaths and ReaderWriter's and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->sharedObjects = vsg::SharedObjects::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());

        arguments.read(options);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgviewer";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        if (arguments.read("--d32")) windowTraits->depthFormat = VK_FORMAT_D32_SFLOAT;
        arguments.read("--screen", windowTraits->screenNum);
        arguments.read("--display", windowTraits->display);
        arguments.read("--samples", windowTraits->samples);

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        if (argc <= 1)
        {
            std::cout << "Please specify a 3d model or image file on the command line." << std::endl;
            return 1;
        }

        auto group = vsg::Group::create();

        vsg::Path path;

        // read any vsg files
        for (int i = 1; i < argc; ++i)
        {
            vsg::Path filename = arguments[i];
            path = vsg::filePath(filename);

            auto object = vsg::read(filename, options);
            if (auto node = object.cast<vsg::Node>())
            {
                group->addChild(node);
            }
            else if (auto contents = object.cast<vsg::stringValue>())
            {
                auto ext = vsg::lowerCaseFileExtension(filename);
                auto& text = contents->value();
                if (ext == ".md")
                {
                    if (auto presentation = parse_md(text))
                    {
                        group->addChild(presentation);
                    }
                }
                else
                {
                    std::cout<<"ext = "<<ext<<std::endl;
                    std::cout<<"text = "<<std::endl<<text<<std::endl;
                }
            }
            else if (object)
            {
                std::cout << "Unable to view object of type " << object->className() << std::endl;
            }
            else
            {
                std::cout << "Unable to load file " << filename << std::endl;
            }
        }

        if (group->children.empty())
        {
            return 1;
        }

        vsg::ref_ptr<vsg::Node> vsg_scene;
        if (group->children.size() == 1)
            vsg_scene = group->children[0];
        else
            vsg_scene = group;

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.001;

        // set up the camera
        auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel"));
        if (ellipsoidModel)
        {
            double horizonMountainHeight = 0.0;
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
        }
        else
        {
            perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // add close handler to respond the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
        viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        viewer->compile();

        // rendering main loop
        while (viewer->advanceToNextFrame())
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
        }
    }
    catch (const vsg::Exception& ve)
    {
        for (int i = 0; i < argc; ++i) std::cerr << argv[i] << " ";
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        return 1;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
