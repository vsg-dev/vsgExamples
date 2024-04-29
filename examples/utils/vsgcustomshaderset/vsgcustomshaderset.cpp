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
    windowTraits->windowTitle = "vscustomshaderset";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    windowTraits->synchronizationLayer = arguments.read("--sync");

    auto numFrames = arguments.value(-1, "-f");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");
    auto outputShaderSetFilename = arguments.value<vsg::Path>("", "--os");
    auto horizonMountainHeight = arguments.value(0.0, "--hmh");
    auto nearFarRatio = arguments.value<double>(0.001, "--nfr");
    bool reportAverageFrameRate = arguments.read("--fps");
    bool inherit = arguments.read("--inherit");

    vsg::ref_ptr<vsg::ShaderSet> shaderSet = custom::pbr_ShaderSet(options);
    options->shaderSets["pbr"] = shaderSet;
    if (!shaderSet)
    {
        std::cout << "No vsg::ShaderSet to process." << std::endl;
        return 1;
    }

    bool fogSet = false;
    auto fogValue = custom::FogValue::create();
    auto& fog = fogValue->value();
    fogSet = arguments.read("--color", fog.color) | fogSet;
    fogSet = arguments.read("--start", fog.start) | fogSet;
    fogSet = arguments.read("--end", fog.end) | fogSet;
    fogSet = arguments.read("--exponent", fog.exponent) | fogSet;
    fogSet = arguments.read("--density", fog.density) | fogSet;

    if (fogSet && !inherit)
    {
        // change the default data to the local fogValue we've been assigning values to
        auto& fogBinding = shaderSet->getDescriptorBinding("Fog");
        if (fogBinding.name == "Fog")
        {
            fogBinding.data = fogValue;
            vsg::info("Setting default fog to ", fogValue);
        }
    }

    if (outputShaderSetFilename)
    {
        vsg::write(shaderSet, outputShaderSetFilename, options);
    }

    auto vsg_scene = vsg::StateGroup::create();
    if (inherit)
    {
        //
        // place the FogValue DescriptorSet at the root of the scene graph
        // and tell the loader via vsg::Options::inheritedState that it doesn't need to assign
        // it to subgraphs that loader will create
        //
        auto layout = shaderSet->createPipelineLayout({}, {0, 2});

        uint32_t vds_set = 1;
        vsg_scene->add(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, vds_set));

        uint32_t cm_set = 0;
        auto cm_dsl = shaderSet->createDescriptorSetLayout({}, cm_set);
        auto cm_db = vsg::DescriptorBuffer::create(fogValue);
        auto cm_ds = vsg::DescriptorSet::create(cm_dsl, vsg::Descriptors{cm_db});
        auto cm_bds = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, cm_ds);
        vsg_scene->add(cm_bds);

        vsg::info("Added state to inherit ");
        options->inheritedState = vsg_scene->stateCommands;
    }

    vsg::Path path;

    // read any vsg files
    for (int i = 1; i < argc; ++i)
    {
        vsg::Path filename = arguments[i];
        if (auto node = vsg::read_cast<vsg::Node>(filename, options))
        {
            vsg_scene->addChild(node);
        }
        else
        {
            std::cout << "Unable to load file " << filename << std::endl;
        }
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
