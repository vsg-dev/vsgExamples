/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield
Copyright(c) 2020 Tim Moore

Portions derived from code that is Copyright (C) Sascha Willems - www.saschawillems.de

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/all.h>

#ifdef USE_VSGXCHANGE
#include <vsgXchange/ReaderWriter_all.h>
#include <vsgXchange/ShaderCompiler.h>
#endif

#include <iostream>
#include <chrono>
#include <thread>

#include "../shared/AnimationPath.h"

vsg::ref_ptr<vsg::RenderGraph> createRenderGraph(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, VkFormat imageFormat, VkFormat depthFormat)
{
    VkExtent3D attachmentExtent{extent.width, extent.height, 1};
    // Attachments
    // create image for color attachment
    VkImageCreateInfo colorImageCreateInfo;
    colorImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    colorImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    colorImageCreateInfo.format = imageFormat;
    colorImageCreateInfo.extent = attachmentExtent;
    colorImageCreateInfo.mipLevels = 1;
    colorImageCreateInfo.arrayLayers = 1;
    colorImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    colorImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    colorImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImageCreateInfo.flags = 0;
    colorImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    colorImageCreateInfo.queueFamilyIndexCount = 0;
    colorImageCreateInfo.pNext = nullptr;
    auto colorImageView = vsg::createImageView(device, colorImageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT);

    // create depth buffer
    VkImageCreateInfo depthImageCreateInfo = {};
    depthImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageCreateInfo.extent = attachmentExtent;
    depthImageCreateInfo.mipLevels = 1;
    depthImageCreateInfo.arrayLayers = 1;
    depthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageCreateInfo.format = depthFormat;
    depthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImageCreateInfo.flags = 0;
    depthImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthImageCreateInfo.pNext = nullptr;

    auto depthImageView = vsg::createImageView(device, depthImageCreateInfo, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    auto renderPass = vsg::createRenderPass(device, imageFormat, depthFormat);

    auto framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{colorImageView, depthImageView}, extent.width, extent.height, 1);

    auto renderGraph = vsg::RenderGraph::create();

    renderGraph->framebuffer = framebuffer;

    renderGraph->renderArea.offset = {0, 0};
    renderGraph->renderArea.extent = extent;

    renderGraph->clearValues.resize(2);
    renderGraph->clearValues[0].color = { {0.2f, 0.2f, 0.4f, 1.0f} };
    renderGraph->clearValues[1].depthStencil = VkClearDepthStencilValue{1.0f, 0};

    return renderGraph;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    VkExtent2D extent{1920, 1020};
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

    vsg::CommandLine arguments(&argc, argv);
    arguments.read({"--extent", "-w"}, extent.width, extent.height);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto databasePager = vsg::DatabasePager::create_if( arguments.read("--pager") );
    auto numFrames = arguments.value(100, "-f");
    auto pathFilename = arguments.value(std::string(),"-p");
    auto multiThreading = arguments.read("--mt");
    if (arguments.read("--st")) extent = VkExtent2D{192,108};

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    if (argc<=1)
    {
        std::cout<<"Please specify model to load on command line"<<std::endl;
        return 1;
    }

    auto options = vsg::Options::create();
#ifdef USE_VSGXCHANGE
    // add use of vsgXchange's support for reading and writing 3rd party file formats
    options->readerWriter = vsgXchange::ReaderWriter_all::create();
#endif

    auto vsg_scene = vsg::read_cast<vsg::Node>(argv[1], options);
    if (!vsg_scene)
    {
        std::cout<<"No command graph created."<<std::endl;
        return 1;
    }


    // create instance
    vsg::Names instanceExtensions;
    vsg::Names requestedLayers;
    if (debugLayer || apiDumpLayer)
    {
        instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        requestedLayers.push_back("VK_LAYER_LUNARG_standard_validation");
        if (apiDumpLayer) requestedLayers.push_back("VK_LAYER_LUNARG_api_dump");
    }

    vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);

    auto instance = vsg::Instance::create(instanceExtensions, validatedNames, nullptr);
    auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (!physicalDevice || queueFamily < 0)
    {
        std::cout<<"Could not create PhysicalDevice"<<std::endl;
        return 0;
    }

    vsg::Names deviceExtensions;
    vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}};
    auto device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, nullptr);


    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel")); ellipsoidModel)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height), nearFarRatio, 0.0);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height), nearFarRatio*radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));

    // set up the Rendergraph to manage the rendering
    auto renderGraph = createRenderGraph(device, extent, imageFormat, depthFormat);
    renderGraph->camera = camera;
    renderGraph->addChild(vsg_scene);

    auto commandGraph = vsg::CommandGraph::create(device, queueFamily);
    commandGraph->addChild(renderGraph);

    // create the viewer
    auto viewer = vsg::Viewer::create();

    if (!pathFilename.empty())
    {
        std::ifstream in(pathFilename);
        if (!in)
        {
            std::cout << "AnimationPat: Could not open animation path file \"" << pathFilename << "\".\n";
            return 1;
        }

        vsg::ref_ptr<vsg::AnimationPath> animationPath(new vsg::AnimationPath);
        animationPath->read(in);

        viewer->addEventHandler(vsg::AnimationPathHandler::create(camera, animationPath, viewer->start_point()));
    }


    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph}, databasePager);

    viewer->compile();

    if (multiThreading)
    {
        viewer->setupThreading();
    }

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames--)>0)
    {
        // pass any events into EventHandlers assigned to the Viewer, this includes Frame events generated by the viewer each frame
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
