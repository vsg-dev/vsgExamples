#include <iostream>
#include <vsg/all.h>
#include <vsgXchange/all.h>
#include <vsgXchange/gltf.h>

namespace custom
{
    vsg::ref_ptr<vsg::ShaderSet> pbr_ShaderSet(vsg::ref_ptr<const vsg::Options> options)
    {
        vsg::info("Local pbr_ShaderSet(", options, ")");

        auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard.vert", options);
        auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/wireframe_pbr.frag", options);

        if (!vertexShader || !fragmentShader)
        {
            vsg::error("pbr_ShaderSet(...) could not find shaders.");
            return {};
        }

        // use the built PBR ShaderSet as a starting place
        auto shaderSet = vsg::createPhysicsBasedRenderingShaderSet(options);

        // replace the shader stages with our custom wireframe_pbr fragment shader
        shaderSet->stages = vsg::ShaderStages{vertexShader, fragmentShader};

        // clear precompiled shader variants to force use of the new shader stages.
        shaderSet->variants.clear();

        return shaderSet;
    }

    class MeshGltfBuilder : public vsg::Inherit<vsgXchange::gltf::Builder, MeshGltfBuilder>
    {
    public:
        MeshGltfBuilder()
        {
        }

        MeshGltfBuilder(const MeshGltfBuilder&, const vsg::CopyOp& = {}) :
        Inherit()
        {
        }

        vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp& copyop = {}) const override
        {
            return MeshGltfBuilder::create(*this, copyop);
        }


        vsg::ref_ptr<vsg::Object> createSceneGraph(vsg::ref_ptr<vsgXchange::gltf::glTF> in_model, vsg::ref_ptr<const vsg::Options> in_options) override
        {
            vsg::info("MeshGltfBuilder::createSceneGraph(", in_model, ", ", in_options,") ", this);
            return Inherit::createSceneGraph(in_model, in_options);
        }

        vsg::ref_ptr<vsg::Node> createMesh(vsg::ref_ptr<vsgXchange::gltf::Mesh> gltf_mesh, const MeshExtras& meshExtras)
        {
            vsg::info("MeshGltfBuilder::createMesh(", gltf_mesh, ", ..) ", this);
            return Inherit::createMesh(gltf_mesh, meshExtras);
        }
    };
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create(arguments);

    // use the vsg::Options object to pass the ReaderWriter_all to use when reading files.
    auto options = vsg::Options::create();
    options->add(vsgXchange::all::create());
    options->setObject(vsgXchange::gltf::prototype_builder, custom::MeshGltfBuilder::create());

    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

    // read any command line options that the ReaderWriters support
    options->readOptions(arguments);

    auto numFrames = arguments.value(-1, "-f");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");
    auto outputShaderSetFilename = arguments.value<vsg::Path>("", "--os");
    auto horizonMountainHeight = arguments.value(0.0, "--hmh");
    auto nearFarRatio = arguments.value<double>(0.001, "--nfr");
    bool reportAverageFrameRate = arguments.read("--fps");

    vsg::ref_ptr<vsg::ShaderSet> shaderSet = custom::pbr_ShaderSet(options);
    options->shaderSets["pbr"] = shaderSet;
    if (!shaderSet)
    {
        std::cout << "No vsg::ShaderSet to process." << std::endl;
        return 1;
    }

    if (outputShaderSetFilename)
    {
        vsg::write(shaderSet, outputShaderSetFilename, options);
    }

    auto vsg_scene = vsg::Group::create();

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

    windowTraits->vulkanVersion = VK_API_VERSION_1_1;
    windowTraits->deviceExtensionNames = {VK_EXT_MESH_SHADER_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};
    windowTraits->deviceExtensionNames.push_back(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);

    // set up features
    auto& features = windowTraits->deviceFeatures;

    auto& meshFeatures = features->get<VkPhysicalDeviceMeshShaderFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT>();
    meshFeatures.meshShader = VK_TRUE;
    meshFeatures.taskShader = VK_TRUE;

    auto& barycentricFeatures = features->get<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR>();
    barycentricFeatures.fragmentShaderBarycentric = VK_TRUE;

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
