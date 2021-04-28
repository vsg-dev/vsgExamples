#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

// Example that uses gl_ClipDistancep[] to do GPU clipping of primitives
// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_ClipDistance.xhtml
// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_CullDistance.xhtml

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgmmeshshader";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        arguments.read("--screen", windowTraits->screenNum);
        auto type = arguments.value<int>(0, {"--type", "-t"});
        auto outputFilename = arguments.value<vsg::Path>("", "-o");

        auto numFrames = arguments.value(-1, "-f");
        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        // set up search paths to SPIRV shaders and textures
        auto options = vsg::Options::create();
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    #ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
    #endif

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        // set up features
        auto requestFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();

        requestFeatures->get().samplerAnisotropy = VK_TRUE;
        requestFeatures->get().shaderClipDistance = VK_TRUE;

        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        // get the Window to create the Instance & PhysicalDevice for us.
        auto& availableFeatures = window->getOrCreatePhysicalDevice()->getFeatures(); // VkPhysicalDeviceFeatures
        auto& limits = window->getOrCreatePhysicalDevice()->getProperties().limits; // VkPhysicalDeviceLimits

        std::cout<<"availableFeatures.samplerAnisotropy = "<<availableFeatures.samplerAnisotropy<<", limits.maxSamplerAnisotropy = "<<limits.maxSamplerAnisotropy<<std::endl;
        std::cout<<"availableFeatures.shaderClipDistance = "<<availableFeatures.shaderClipDistance<<", limits.maxClipDistances = "<<limits.maxClipDistances<<std::endl;
        std::cout<<"availableFeatures.shaderCullDistance = "<<availableFeatures.shaderCullDistance<<", limits.maxCullDistances = "<<limits.maxCullDistances<<std::endl;
        std::cout<<"limits.maxCombinedClipAndCullDistances = "<<limits.maxCombinedClipAndCullDistances<<std::endl;

        if (!availableFeatures.samplerAnisotropy || !availableFeatures.shaderClipDistance)
        {
            std::cout<<"Required features not supported."<<std::endl;
            return 1;
        }
    }
    catch (const vsg::Exception& exception)
    {
        std::cout << exception.message << " VkResult = " << exception.result << std::endl;
        return 0;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
