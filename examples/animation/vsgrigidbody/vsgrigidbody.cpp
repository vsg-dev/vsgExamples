#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

using AnimationGroups = std::vector<vsg::ref_ptr<vsg::AnimationGroup>>;

class AnimationManager : public vsg::Inherit<vsg::Operation, AnimationManager>
{
public:

    AnimationGroups animationGroups;

    vsg::ref_ptr<vsg::FrameStamp> frameStamp;

    void start(vsg::ref_ptr<vsg::AnimationGroup> animationGroup, vsg::ref_ptr<vsg::Animation> animation)
    {
        vsg::info("AnimationManager::start(", animationGroup, ", ", animation, ", ", animation->name, ")");
    }

    void end(vsg::ref_ptr<vsg::AnimationGroup> animationGroup, vsg::ref_ptr<vsg::Animation> animation)
    {
        vsg::info("AnimationManager::start(", animationGroup, ", ", animation, ", ", animation->name, ")");
    }

    void run() override
    {
        // vsg::info("AnimationManager::run() ", animationGroups.size());
    }
};

class FindAnimation : public vsg::Inherit<vsg::Visitor, FindAnimation>
{
public:

    AnimationGroups animationGroups;

    void apply(vsg::Node& node) override
    {
        //vsg::info(node.className());
        node.traverse(*this);
    }

    void apply(vsg::Animation& animation) override
    {
        vsg::info("Animation ", animation.className(), " ", animation.name);
        animation.traverse(*this);
    }

    void apply(vsg::AnimationGroup& node) override
    {
        vsg::info("AnimationGroup ", node.className(), " node.animations.size() = ", node.animations.size());

        for(auto& animation : node.animations)
        {
            animation->accept(*this);
        }

        animationGroups.emplace_back(&node);

        node.traverse(*this);
    }
};

class AnimationControl : public vsg::Inherit<vsg::Visitor, AnimationControl>
{
public:

    AnimationControl(vsg::ref_ptr<AnimationManager> am) : animationManager(am)
    {
        for(auto ag : am->animationGroups)
        {
            for(auto& animation : ag->animations)
            {
                animations.emplace_back(ag, animation);
            }
        }
        itr = animations.begin();
    }

    using AnimationPairs = std::vector<std::pair<vsg::ref_ptr<vsg::AnimationGroup>, vsg::ref_ptr<vsg::Animation>>>;

    vsg::ref_ptr<AnimationManager> animationManager;
    AnimationPairs animations;
    AnimationPairs::iterator itr;

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyModified == 'p')
        {
            if (itr != animations.end())
            {
                animationManager->start(itr->first, itr->second);

                ++itr;
                if (itr == animations.end()) itr = animations.begin();
            }
        }
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
    windowTraits->windowTitle = "vsgskinning";

    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});

    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    auto numFrames = arguments.value(-1, "-f");
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }

    // bool useStagingBuffer = arguments.read({"--staging-buffer", "-s"});

    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    if (argc<=1)
    {
        std::cout<<"Please specify model to load on command line."<<std::endl;
        return 0;
    }

    vsg::Path filename = argv[1];
    auto model = vsg::read_cast<vsg::Node>(filename, options);
    if (!model)
    {
        std::cout<<"Failed to load "<<filename<<std::endl;
        return 1;
    }

    FindAnimation findAnimation;
    model->accept(findAnimation);

    auto animationManager = AnimationManager::create();
    animationManager->animationGroups = findAnimation.animationGroups;

    // compute the bounds of the scene graph to help position camera
    auto bounds = vsg::visit<vsg::ComputeBounds>(model).bounds;
    vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
    double radius = vsg::length(bounds.max - bounds.min) * 0.6;

    auto scene = model;

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
    bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
    centre = (bounds.min + bounds.max) * 0.5;
    radius = vsg::length(bounds.max - bounds.min) * 0.6;

    // set up the camera
    lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    double nearFarRatio = 0.001;
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.0);

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add the camera and scene graph to View
    auto view = vsg::View::create();
    view->camera = camera;
    view->addChild(scene);

    // assign the animationManager to viewer as an update operation.
    viewer->addUpdateOperation(animationManager, vsg::UpdateOperations::ALL_FRAMES);

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(camera));
    viewer->addEventHandler(AnimationControl::create(animationManager));

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

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
