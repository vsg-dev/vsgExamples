#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include "custom_pbr.h"

int main(int argc, char** argv)
{
    // use the vsg::Options object to pass the ReaderWriter_all to use when reading files.
    auto options = vsg::Options::create();
#ifdef vsgXchange_FOUND
    options->add(vsgXchange::all::create());
#endif
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // read any command line options that the ReaderWriters support
    arguments.read(options);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgincompatiblepipelinelayouts";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    windowTraits->synchronizationLayer = arguments.read("--sync");

    auto numFrames = arguments.value(-1, "-f");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");
    auto horizonMountainHeight = arguments.value(0.0, "--hmh");
    auto nearFarRatio = arguments.value<double>(0.001, "--nfr");
    bool reportAverageFrameRate = arguments.read("--fps");
    bool inherit = arguments.read("--inherit");
    bool text_layer = arguments.read("--text-layer");
    bool highlight = arguments.read("--highlight");
    bool first = arguments.read("--first");
    bool last = arguments.read("--last");

    vsg::ref_ptr<vsg::ShaderSet> customShaderSet = custom::pbr_ShaderSet(options);
    if (highlight)
    {
        if (!customShaderSet->defaultShaderHints)
            customShaderSet->defaultShaderHints = vsg::ShaderCompileSettings::create();
        customShaderSet->defaultShaderHints->defines.insert("CUSTOM_HIGHLIGHT");
    }
    options->shaderSets["pbr"] = customShaderSet;
    if (!customShaderSet)
    {
        std::cout << "No vsg::ShaderSet to process." << std::endl;
        return 1;
    }

    const auto& defines = customShaderSet->defaultShaderHints ? customShaderSet->defaultShaderHints->defines : std::set<std::string>{};

    bool fogSet = false;
    auto fogValue = custom::FogValue::create();
    auto& fog = fogValue->value();
    fogSet = arguments.read("--color", fog.color) || fogSet;
    fogSet = arguments.read("--start", fog.start) || fogSet;
    fogSet = arguments.read("--end", fog.end) || fogSet;
    fogSet = arguments.read("--exponent", fog.exponent) || fogSet;
    fogSet = arguments.read("--density", fog.density) || fogSet;

    if (fogSet && !inherit)
    {
        // change the default data to the local fogValue we've been assigning values to
        auto& fogBinding = customShaderSet->getDescriptorBinding("Fog");
        if (fogBinding.name == "Fog")
        {
            fogBinding.data = fogValue;
            vsg::info("Setting default fog to ", fogValue);
        }
    }

    auto vsg_scene = vsg::StateGroup::create();

    auto layout = customShaderSet->createPipelineLayout(defines, {0, 4});

    uint32_t highlight_set = 3;
    auto highlight_dsl = customShaderSet->createDescriptorSetLayout(defines, highlight_set);

    if (inherit)
    {
        //
        // place the FogValue DescriptorSet at the root of the scene graph
        // and optionally tell the loader via vsg::Options::inheritedState that it doesn't need to assign
        // it to subgraphs that loader will create
        //
        uint32_t cm_set = 0;
        auto cm_dsl = customShaderSet->createDescriptorSetLayout(defines, cm_set);
        auto cm_db = vsg::DescriptorBuffer::create(fogValue);
        auto cm_ds = vsg::DescriptorSet::create(cm_dsl, vsg::Descriptors{cm_db});
        auto cm_bds = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, cm_set, cm_ds);
        vsg_scene->add(cm_bds);

        uint32_t vds_set = 1;
        vsg_scene->add(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, vds_set));

        vsg::info("Added state to inherit ");
        if (highlight)
        {
            // Copy the container so we can add more inherited state without setting an overall value for the whole scenegraph
            options->inheritedState = vsg::StateCommands(vsg_scene->stateCommands);
            options->inheritedState.push_back(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, highlight_set, nullptr));
        }
        else
            options->inheritedState = vsg_scene->stateCommands;
    }

    // read any vsg files
    for (int i = 1; i < argc; ++i)
    {
        vsg::Path filepath = arguments[i];
        auto group = vsg::StateGroup::create();

        if (highlight)
        {
            auto highlight_db = vsg::DescriptorBuffer::create(custom::HighlightValue::create(custom::Highlight{i % 2 == 1}));
            auto highlight_ds = vsg::DescriptorSet::create(highlight_dsl, vsg::Descriptors{highlight_db});
            auto highlight_bds = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, highlight_set, highlight_ds);
            group->add(highlight_bds);
        }

        vsg::ref_ptr<vsg::Node> node;
        if ((node = vsg::read_cast<vsg::Node>(filepath, options)))
        {
            if (first)
                group->addChild(node);
        }
        else
        {
            std::cout << "Unable to load file " << filepath << std::endl;
        }

        auto text = vsg::Text::create();
        text->font = vsg::read_cast<vsg::Font>("fonts/times.vsgb", options);
        auto textLayout = vsg::StandardLayout::create();
        textLayout->billboard = true;
        text->layout = textLayout;
        std::string filename = filepath.string();
        auto filenameStart = filename.find_last_of("\\/");
        if (filenameStart != std::string::npos)
            filename = filename.substr(filenameStart + 1);
        text->text = vsg::stringValue::create(filename);
        text->setup(0, options);
        if (text_layer)
        {
            auto layer = vsg::Layer::create();
            layer->child = text;
            group->addChild(layer);
        }
        else
            group->addChild(text);

        if (node && last)
        {
            group->addChild(node);
        }

        vsg_scene->addChild(group);
    }

    if (vsg_scene->children.empty())
    {
        return 1;
    }

    if (outputFilename)
    {
        vsg::write(vsg_scene, outputFilename, options);
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // compute the bounds of the scene graph to help position camera
    auto bounds = vsg::visit<vsg::ComputeBounds>(vsg_scene).bounds;
    vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
    double radius = vsg::length(bounds.max - bounds.min) * 0.6;

    vsg::info("scene bounds ", bounds);

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
    if (ellipsoidModel)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

    viewer->start_point() = vsg::clock::now();

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    if (reportAverageFrameRate)
    {
        auto fs = viewer->getFrameStamp();
        double fps = static_cast<double>(fs->frameCount) / std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - viewer->start_point()).count();
        std::cout << "Average frame rate = " << fps << " fps" << std::endl;
    }
    return 0;
}
