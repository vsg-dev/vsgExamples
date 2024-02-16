#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#ifdef Tracy_FOUND
#    include <vsg/utils/TracyInstrumentation.h>
#endif


#include <iostream>


class AnimationControl : public vsg::Inherit<vsg::Visitor, AnimationControl>
{
public:

    AnimationControl(vsg::ref_ptr<vsg::AnimationManager> am, const vsg::AnimationGroups& in_animationGroups) : animationManager(am), animationGroups(in_animationGroups)
    {
        for(auto ag : animationGroups)
        {
            animations.insert(animations.end(), ag->animations.begin(), ag->animations.end());
        }
        itr = animations.end();
    }

    vsg::ref_ptr<vsg::AnimationManager> animationManager;
    vsg::AnimationGroups animationGroups;
    vsg::Animations animations;
    vsg::Animations::iterator itr;

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (animations.empty()) return;

        if (keyPress.keyModified == 'p')
        {
            keyPress.handled = true;

            // if the iterator hasn't been set yet, set it to the next animation after the last one played,
            // or the first entry in the list of animations
            if (itr == animations.end())
            {
                if (!(animationManager->animations.empty()))
                {
                    itr = std::find(animations.begin(), animations.end(), animationManager->animations.back());
                    if (itr != animations.end()) ++itr;
                }

                if (itr == animations.end()) itr = animations.begin();
            }

            // stop all running animations
            animationManager->stop();

            // play the animation
            if (itr != animations.end())
            {
                if (animationManager->play(*itr))
                {
                    std::cout<<"Playing "<<(*itr)->name<<std::endl;

                    ++itr;
                    if (itr == animations.end()) itr = animations.begin();
                }
            }
        }
        else if (keyPress.keyModified == 's')
        {
            keyPress.handled = true;

            // stop all running animations
            animationManager->stop();
        }
        else if (keyPress.keyModified == 'a')
        {
            keyPress.handled = true;

            // stop all running animations
            animationManager->stop();

            for(auto ag : animationGroups)
            {
                if (!ag->animations.empty())
                {
                    animationManager->play(ag->animations.front());
                }
            }
        }
    }
};

/// Find all the objects that should be treated as dynamic (their values change.)
class FindDynamicObjects : public vsg::ConstVisitor
{
public:

    std::set<const Object*> dynamicObjects;

    inline void tag(const vsg::Object* object)
    {
        dynamicObjects.insert(object);
    }

    inline bool tagged(const vsg::Object* object)
    {
        return dynamicObjects.count(object) != 0;
    }

    void apply(const vsg::Object& object) override
    {
        object.traverse(*this);
    }

    void apply(const vsg::Data& data) override
    {
        if (data.properties.dataVariance != vsg::STATIC_DATA) tag(&data);
    }

    void apply(const vsg::AnimationGroup& ag) override
    {
        tag(&ag);
        ag.traverse(*this);
    }

    void apply(const vsg::Animation& animation) override
    {
        tag(&animation);
        animation.traverse(*this);
    }

    void apply(const vsg::AnimationSampler& sampler) override
    {
        tag(&sampler);
    }

    void apply(const vsg::TransformSampler& sampler) override
    {
        tag(&sampler);
        if (sampler.object)
        {
            tag(sampler.object);
            sampler.object->traverse(*this);
        }
    }

    void apply(const vsg::MorphSampler& sampler) override
    {
        tag(&sampler);
        tag(sampler.object);
    }

    void apply(const vsg::JointSampler& sampler) override
    {
        tag(&sampler);
        tag(sampler.jointMatrices);
        tag(sampler.subgraph);
        sampler.subgraph->traverse(*this);
    }

    void apply(const vsg::BufferInfo& info) override
    {
        if (info.data) info.data->accept(*this);
    }

    void apply(const vsg::Image& image) override
    {
        if (image.data) image.data->accept(*this);
    }

    void apply(const vsg::ImageView& imageView) override
    {
        if (imageView.image) imageView.image->accept(*this);
    }

    void apply(const vsg::ImageInfo& info) override
    {
        if (info.sampler) info.sampler->accept(*this);
        if (info.imageView) info.imageView->accept(*this);
    }

    void apply(const vsg::DescriptorBuffer& db) override
    {
        for(auto info : db.bufferInfoList)
        {
            info->accept(*this);
        }
    }

    void apply(const vsg::DescriptorImage& di) override
    {
        for(auto info : di.imageInfoList)
        {
            info->accept(*this);
        }
    }

    void apply(const vsg::BindIndexBuffer& bib) override
    {
        if (bib.indices) bib.indices->accept(*this);
    }

    void apply(const vsg::BindVertexBuffers& bvb) override
    {
        for(auto info : bvb.arrays)
        {
            if (info) info->accept(*this);
        }
    }

    void apply(const vsg::VertexDraw& vd) override
    {
        for(auto info : vd.arrays)
        {
            if (info) info->accept(*this);
        }
    }

    void apply(const vsg::VertexIndexDraw& vid) override
    {
        if (vid.indices) vid.indices->accept(*this);
        for(auto info : vid.arrays)
        {
            if (info) info->accept(*this);
        }
    }

    void apply(const vsg::Geometry& geom) override
    {
        if (geom.indices) geom.indices->accept(*this);
        for(auto info : geom.arrays)
        {
            if (info) info->accept(*this);
        }
        for(auto command : geom.commands)
        {
            if (command) command->accept(*this);
        }
    }
};

/// Propagate classification of objects as dynamic to all parents
class PropogateDynamicObjects : public vsg::ConstVisitor
{
public:

    PropogateDynamicObjects()
    {
        taggedStack.push(false);
    }

    std::set<const Object*> dynamicObjects;
    std::stack<bool> taggedStack;

    inline void tag(const vsg::Object* object)
    {
        dynamicObjects.insert(object);
    }

    inline bool tagged(const vsg::Object* object)
    {
        return dynamicObjects.count(object) != 0;
    }

protected:

    struct TagIfChildIsDynamic
    {
        inline TagIfChildIsDynamic(PropogateDynamicObjects* in_rd, const vsg::Object* in_object) :
            rd(in_rd),
            object(in_object)
        {
            if (rd->tagged(object)) rd->taggedStack.top() = true;
            rd->taggedStack.push(false);
        }

        inline ~TagIfChildIsDynamic()
        {
            if (rd->taggedStack.top())
            {
                rd->tag(object);
                rd->taggedStack.pop();
                rd->taggedStack.top() = true;
            }
            else
            {
                rd->taggedStack.pop();
            }
        }

        PropogateDynamicObjects* rd = nullptr;
        const vsg::Object* object = nullptr;
    };

    void apply(const vsg::Object& object) override
    {
        TagIfChildIsDynamic t(this, &object);
        object.traverse(*this);
    }

    void apply(const vsg::AnimationGroup& ag) override
    {
        TagIfChildIsDynamic t(this, &ag);
        ag.traverse(*this);
    }

    void apply(const vsg::Animation& animation) override
    {
        TagIfChildIsDynamic t(this, &animation);
        animation.traverse(*this);
    }

    void apply(const vsg::AnimationSampler& sampler) override
    {
        TagIfChildIsDynamic t(this, &sampler);
    }

    void apply(const vsg::TransformSampler& sampler) override
    {
        TagIfChildIsDynamic t(this, &sampler);
        if (sampler.object) sampler.object->accept(*this);
    }

    void apply(const vsg::MorphSampler& sampler) override
    {
        TagIfChildIsDynamic t(this, &sampler);
    }

    void apply(const vsg::JointSampler& sampler) override
    {
        TagIfChildIsDynamic t(this, &sampler);
        if (sampler.subgraph) sampler.subgraph->accept(*this);
    }

    void apply(const vsg::BufferInfo& info) override
    {
        TagIfChildIsDynamic t(this, &info);
        if (info.data) info.data->accept(*this);
    }

    void apply(const vsg::Image& image) override
    {
        TagIfChildIsDynamic t(this, &image);
        if (image.data) image.data->accept(*this);
    }

    void apply(const vsg::ImageView& imageView) override
    {
        TagIfChildIsDynamic t(this, &imageView);
        if (imageView.image) imageView.image->accept(*this);
    }

    void apply(const vsg::ImageInfo& info) override
    {
        TagIfChildIsDynamic t(this, &info);
        if (info.sampler) info.sampler->accept(*this);
        if (info.imageView) info.imageView->accept(*this);
    }

    void apply(const vsg::DescriptorBuffer& db) override
    {
        TagIfChildIsDynamic t(this, &db);
        for(auto info : db.bufferInfoList)
        {
            info->accept(*this);
        }
    }

    void apply(const vsg::DescriptorImage& di) override
    {
        TagIfChildIsDynamic t(this, &di);
        for(auto info : di.imageInfoList)
        {
            info->accept(*this);
        }
    }

    void apply(const vsg::BindIndexBuffer& bib) override
    {
        TagIfChildIsDynamic t(this, &bib);
        if (bib.indices) bib.indices->accept(*this);
    }

    void apply(const vsg::BindVertexBuffers& bvb) override
    {
        TagIfChildIsDynamic t(this, &bvb);
        for(auto info : bvb.arrays)
        {
            if (info) info->accept(*this);
        }
    }

    void apply(const vsg::VertexDraw& vd) override
    {
        TagIfChildIsDynamic t(this, &vd);
        for(auto info : vd.arrays)
        {
            if (info) info->accept(*this);
        }
    }

    void apply(const vsg::VertexIndexDraw& vid) override
    {
        TagIfChildIsDynamic t(this, &vid);
        if (vid.indices) vid.indices->accept(*this);
        for(auto info : vid.arrays)
        {
            if (info) info->accept(*this);
        }
    }

    void apply(const vsg::Geometry& geom) override
    {
        TagIfChildIsDynamic t(this, &geom);
        if (geom.indices) geom.indices->accept(*this);
        for(auto info : geom.arrays)
        {
            if (info) info->accept(*this);
        }
        for(auto command : geom.commands)
        {
            if (command) command->accept(*this);
        }
    }
};


class PrintSceneGraph : public vsg::Visitor
{
public:
    vsg::indentation indent{4};
    vsg::CopyOp copyop;

    void apply(vsg::Object& object) override
    {
        if (copyop.duplicate->contains(&object))
        {
            std::cout<<indent<<object.className()<<" "<<&object<<" requires duplicate" <<std::endl;
        }
        else
        {
            std::cout<<indent<<object.className()<<" "<<&object<<std::endl;
        }

        indent += 4;
        object.traverse(*this);
        indent -= 4;
    }
};


int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->sharedObjects = vsg::SharedObjects::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    arguments.read(options);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsganimation";

    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});

    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    auto numFrames = arguments.value(-1, "-f");
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    if (arguments.read("--IMMEDIATE")) { windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; }
    if (arguments.read({"-t", "--test"}))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->fullscreen = true;
    }
    if (arguments.read("--st"))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
    }

    vsg::ref_ptr<vsg::Instrumentation> instrumentation;
    if (arguments.read({"--gpu-annotation", "--ga"}) && vsg::isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        windowTraits->debugUtils = true;

        auto gpu_instrumentation = vsg::GpuAnnotation::create();
        if (arguments.read("--func")) gpu_instrumentation->labelType = vsg::GpuAnnotation::SourceLocation_function;

        instrumentation = gpu_instrumentation;
    }
#ifdef Tracy_FOUND
    else if (arguments.read("--tracy"))
    {
        windowTraits->deviceExtensionNames.push_back(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);

        auto tracy_instrumentation = vsg::TracyInstrumentation::create();
        arguments.read("--cpu", tracy_instrumentation->settings->cpu_instumentation_level);
        arguments.read("--gpu", tracy_instrumentation->settings->gpu_instumentation_level);
        instrumentation = tracy_instrumentation;
    }
#endif

    // when using shadows set the depthClampEnable to true;
    bool depthClamp = true;
    if (depthClamp)
    {
        std::cout<<"Enabled depth clamp."<<std::endl;
        auto deviceFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
        deviceFeatures->get().samplerAnisotropy = VK_TRUE;
        deviceFeatures->get().depthClamp = VK_TRUE;

        auto rasterizationState = vsg::RasterizationState::create();
        rasterizationState->depthClampEnable = VK_TRUE;

        auto pbr = options->shaderSets["pbr"] = vsg::createPhysicsBasedRenderingShaderSet(options);
        pbr->defaultGraphicsPipelineStates.push_back(rasterizationState);

        auto phong = options->shaderSets["phong"] = vsg::createPhysicsBasedRenderingShaderSet(options);
        phong->defaultGraphicsPipelineStates.push_back(rasterizationState);

        auto flat = options->shaderSets["flat"] = vsg::createPhysicsBasedRenderingShaderSet(options);
        flat->defaultGraphicsPipelineStates.push_back(rasterizationState);
    }

    auto numCopies = arguments.value<unsigned int>(1, "-n");
    auto autoPlay = !arguments.read({"--no-auto-play", "--nop"});
    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    if (argc<=1)
    {
        std::cout<<"Please specify model to load on command line."<<std::endl;
        return 0;
    }

    auto scene = vsg::Group::create();

    unsigned int numLights = 2;
    auto direction = arguments.value(vsg::dvec3(-1.0, 1.0, -1.0), "--direction");
    auto numShadowMapsPerLight = arguments.value<uint32_t>(3, "--sm");
    auto lambda = arguments.value<double>(0.25, "--lambda");
    double maxShadowDistance = arguments.value<double>(1e8, "--sd");
    if (numLights >= 1)
    {
        auto directionalLight = vsg::DirectionalLight::create();
        directionalLight->name = "directional";
        directionalLight->color.set(1.0, 1.0, 1.0);
        directionalLight->intensity = 0.9;
        directionalLight->direction = direction;
        directionalLight->shadowMaps = numShadowMapsPerLight;
        scene->addChild(directionalLight);

        auto ambientLight = vsg::AmbientLight::create();
        ambientLight->name = "ambient";
        ambientLight->color.set(1.0, 1.0, 1.0);
        ambientLight->intensity = 0.1;
        scene->addChild(ambientLight);
    }


    // load the models
    struct ModelBound
    {
        vsg::ref_ptr<vsg::Node> node;
        vsg::dbox bounds;
    };

    auto findAndPropogateDynamicObjects = [](const vsg::Object* object) -> std::set<const vsg::Object*>
    {
        FindDynamicObjects findDynamicObjects;
        object->accept(findDynamicObjects);

        PropogateDynamicObjects propogateDynamicObjects;
        propogateDynamicObjects.dynamicObjects.swap(findDynamicObjects.dynamicObjects);
        object->accept(propogateDynamicObjects);

        return propogateDynamicObjects.dynamicObjects;
    };

    vsg::CopyOp copyop;
    auto duplicate = copyop.duplicate = new vsg::Duplicate;

    std::list<ModelBound> models;
    for (int i = 1; i < argc; ++i)
    {
        vsg::Path filename = arguments[i];
        if (auto node = vsg::read_cast<vsg::Node>(filename, options))
        {
            auto bounds = vsg::visit<vsg::ComputeBounds>(node).bounds;
            models.push_back(ModelBound{node, bounds});

            if (numCopies > 1)
            {
                auto dynamicObjects = findAndPropogateDynamicObjects(node);
                for(auto& object : dynamicObjects)
                {
                    duplicate->insert(object);
                }
            }
        }
    }

    vsg::info("Final number of objects requiring duplication ", copyop.duplicate->size());

    //PrintSceneGraph printer;
    PrintSceneGraph printer;
    printer.copyop = copyop;
    for(auto& model : models)
    {
        model.node->accept(printer);
    }

    std::cout<<std::endl;

    if (models.empty())
    {
        std::cout<<"Failed to load any models, please specify filenames of 3d models on the command line,"<<std::endl;
        return 1;
    }

    if (models.size()==1 && numCopies==1)
    {
        // just a single model so just add it directly to the scene
        scene->addChild(models.front().node);
    }
    else
    {
        // find the largest model diameter so we can use it to set up layout
        double maxDiameter = 0.0;
        for(auto model : models)
        {
            auto diameter = vsg::length(model.bounds.max - model.bounds.min);
            if (diameter > maxDiameter) maxDiameter = diameter;
        }

        // we have multiple models so we need to lay then out on a grid
        unsigned int totalNumberOfModels = static_cast<unsigned int>(models.size()) * numCopies;
        unsigned int columns = static_cast<unsigned int>(std::ceil(std::sqrt(static_cast<double>(totalNumberOfModels))));

        unsigned int column = 0;
        vsg::dvec3 position = {0.0, 0.0, 0.0};
        double spacing = maxDiameter;

        for(unsigned int ci = 0; ci < numCopies; ++ci)
        {
            for(auto [node, bounds] : models)
            {
                auto diameter = vsg::length(bounds.max - bounds.min);
                vsg::dvec3 origin((bounds.min.x + bounds.max.x) * 0.5,
                                  (bounds.min.y + bounds.max.y) * 0.5,
                                  bounds.min.z);
                auto transform = vsg::MatrixTransform::create(vsg::translate(position) * vsg::scale(maxDiameter / diameter) * vsg::translate(-origin));

                if (ci > 0)
                {
                    transform->addChild(copyop(node)); // node->clone(copyop);
                }
                else
                {
                    transform->addChild(node);
                }

                scene->addChild(transform);

                // advance position
                position.x += spacing;
                ++column;
                if (column >= columns)
                {
                    position.x = 0.0;
                    position.y += spacing;
                    column = 0;
                }
            }

            copyop.duplicate->reset();
        }
    }

    // find the animations available in scene
    vsg::FindAnimations findAnimations;
    scene->accept(findAnimations);

    auto animations = findAnimations.animations;
    auto animationGroups = findAnimations.animationGroups;

    std::cout<<"Model contains "<<animations.size()<<" animations."<<std::endl;
    for(auto& ag : animationGroups)
    {
        std::cout<<"AnimationGroup "<<ag<<std::endl;
        for(auto animation : ag->animations)
        {
            std::cout<<"    animation : "<<animation->name<<std::endl;
        }
    }

    // compute the bounds of the scene graph to help position camera
    auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
    vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
    double radius = vsg::length(bounds.max - bounds.min) * 0.6;

    bool insertBaseGeometry = true;
    if (insertBaseGeometry)
    {
        auto builder = vsg::Builder::create();
        builder->options = options;

        vsg::GeometryInfo geomInfo;
        vsg::StateInfo stateInfo;

        double margin = bounds.max.z - bounds.min.z;
        geomInfo.position.set((bounds.min.x + bounds.max.x)*0.5, (bounds.min.y + bounds.max.y)*0.5, bounds.min.z);
        geomInfo.dx.set(bounds.max.x - bounds.min.x + margin, 0.0, 0.0);
        geomInfo.dy.set(0.0, bounds.max.y - bounds.min.y + margin, 0.0);
        geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);

        stateInfo.two_sided = true;

        scene->addChild(builder->createQuad(geomInfo, stateInfo));
    }


    // write out scene if required
    if (outputFilename)
    {
        vsg::write(scene, outputFilename, options);
        return 0;
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

    vsg::ref_ptr<vsg::LookAt> lookAt;

    // update bounds to new scene extent
    centre = (bounds.min + bounds.max) * 0.5;
    radius = vsg::length(bounds.max - bounds.min) * 0.6;
    if (maxShadowDistance == 1e8) maxShadowDistance = radius * 4.0;

    // set up the camera
    lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    double nearFarRatio = 0.001;
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.0);

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add the camera and scene graph to View
    auto view = vsg::View::create();
    view->viewDependentState->maxShadowDistance = maxShadowDistance;
    view->viewDependentState->lambda = lambda;
    view->camera = camera;
    view->addChild(scene);

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(camera));
    viewer->addEventHandler(AnimationControl::create(viewer->animationManager, animationGroups));

    auto renderGraph = vsg::RenderGraph::create(window, view);
    auto commandGraph = vsg::CommandGraph::create(window, renderGraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    if (instrumentation) viewer->assignInstrumentation(instrumentation);

    viewer->compile();

    // start first animation if available
    if (autoPlay && !animations.empty())
    {
        for(auto ag : animationGroups)
        {
            if (!ag->animations.empty()) viewer->animationManager->play(ag->animations.front());
        }
    }

    auto startTime = vsg::clock::now();
    double numFramesCompleted = 0.0;

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();

        numFramesCompleted += 1.0;
    }

    auto duration = std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - startTime).count();
    if (numFramesCompleted > 0.0)
    {
        std::cout << "Average frame rate = " << (numFramesCompleted / duration) << std::endl;
    }

    return 0;
}
