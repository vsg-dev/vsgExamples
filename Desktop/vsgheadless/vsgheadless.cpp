/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield
Copyright(c) 2020 Tim Moore

Portions derived from code that is Copyright (C) Sascha Willems - www.saschawillems.de

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/all.h>

#include <iostream>
#include <chrono>
#include <thread>

// Render a scene to an image, then use the image as a texture on the
// faces of quads. This is based on Sascha William's offscreenrender
// example. 
//
// In VSG / Vulkan terms, we first create a frame buffer that uses
// the image as an attachment. Next, create a RenderGraph that uses that
// frame buffer and the scene graph for the scene. Another RenderGraph
// contains the scene graph for the quads. The quads use the rendered
// image's descriptor. Finally, create the command graph that records those
// two render graphs.
//
// In order for this to work correctly in Vulkan, the subpass
// dependencies of the offscreen RenderPass / RenderGraph need to be
// set up so that the second RenderGraph can sample the output.

// Rendergraph for rendering to image

vsg::ref_ptr<vsg::RenderGraph> createRenderGraph(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent)
{
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;


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

    renderGraph->renderPass = renderPass;
    renderGraph->framebuffer = framebuffer;

    renderGraph->renderArea.offset = {0, 0};
    renderGraph->renderArea.extent = extent;

    renderGraph->clearValues.resize(2);
    renderGraph->clearValues[0].color = { {0.2f, 0.2f, 0.4f, 1.0f} };
    renderGraph->clearValues[1].depthStencil = VkClearDepthStencilValue{1.0f, 0};

    return renderGraph;
}

vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* scenegraph, const VkExtent2D& extent)
{
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0),
                                      centre, vsg::dvec3(0.0, 0.0, 1.0));

    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height),
                                                nearFarRatio*radius, radius * 4.5);

    return vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto useDatabasePager = arguments.read("--pager");
    auto maxPageLOD = arguments.value(-1, "--max-plod");
    bool multiThreading = arguments.read("--mt");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    if (argc<=1)
    {
        std::cout<<"Please specify model to load on command line"<<std::endl;
        return 1;
    }

    auto vsg_scene = vsg::read_cast<vsg::Node>(argv[1]);
    if (!vsg_scene)
    {
        std::cout<<"No command graph created."<<std::endl;
        return 1;
    }

    // A hack for getting the example teapot into the correct orientation
    auto zUp = vsg::MatrixTransform::create(vsg::dmat4(1.0, 0.0, 0.0, 0.0,
                                                       0.0, 0.0, -1.0, 0.0,
                                                       0.0, 1.0, 0.0, 0.0,
                                                       0.0, 0.0, 0.0, 1.0));
    zUp->addChild(vsg_scene);

    // Transform for rotation animation
    auto transform = vsg::MatrixTransform::create();
    transform->addChild(zUp);
    vsg_scene = transform;
    
    // set up database pager
    vsg::ref_ptr<vsg::DatabasePager> databasePager;
    if (useDatabasePager)
    {
        databasePager = vsg::DatabasePager::create();
        if (maxPageLOD>=0) databasePager->targetMaxNumPagedLODWithHighResSubgraphs = maxPageLOD;
    }


    // create instance
    vsg::Names instanceExtensions;
    vsg::Names requestedLayers;
    if (debugLayer)
    {
        instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        requestedLayers.push_back("VK_LAYER_LUNARG_standard_validation");
        if (apiDumpLayer) requestedLayers.push_back("VK_LAYER_LUNARG_api_dump");
    }

    vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);
    auto instance = vsg::Instance::create(instanceExtensions, validatedNames, nullptr);


    std::cout<<"instance = "<<instance<<std::endl;

    auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (!physicalDevice || queueFamily < 0)
    {
        std::cout<<"Could not create PhysicalDevice"<<std::endl;
        return 0;
    }

    vsg::Names deviceExtensions;
    vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}};
    auto device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, nullptr);

    std::cout<<"device = "<<device<<std::endl;

    VkExtent2D extent{1920, 1020};

    auto camera = createCameraForScene(vsg_scene, extent);

    auto renderGraph = createRenderGraph(device, extent);
    renderGraph->camera = camera;
    renderGraph->addChild(vsg_scene);

    auto commandGraph = vsg::CommandGraph::create(device, queueFamily);
    commandGraph->addChild(renderGraph);

    // create the viewer
    auto viewer = vsg::Viewer::create();

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph}, databasePager);


    viewer->compile();

    if (multiThreading)
    {
        viewer->setupThreading();
    }

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        // animate the offscreen scenegraph

        float time = std::chrono::duration<float, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();
        transform->setMatrix(vsg::rotate(time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f)));

        std::cout<<"viewer->getFrameStamp() "<<viewer->getFrameStamp()->frameCount<<" "<<time<<std::endl;

        viewer->update();

        viewer->recordAndSubmit();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
