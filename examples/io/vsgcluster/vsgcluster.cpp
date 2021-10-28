#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

#include "Broadcaster.h"
#include "Receiver.h"
#include "Packet.h"

namespace cluster
{

class ViewerData : public vsg::Inherit<vsg::Object, ViewerData>
{
public:

    bool alive = true;
    vsg::ref_ptr<vsg::FrameStamp> frameStamp;
    vsg::ref_ptr<vsg::LookAt> lookAt;

    void read(vsg::Input& input) override
    {
        vsg::Object::read(input);

        if (!frameStamp) frameStamp = vsg::FrameStamp::create();
        if (!lookAt) lookAt = vsg::LookAt::create();

        input.read("alive", alive);
        input.read("frameCount", frameStamp->frameCount);
        input.read("lookAt.eye", lookAt->eye);
        input.read("lookAt.center", lookAt->center);
        input.read("lookAt.up", lookAt->up);
    }

    void write(vsg::Output& output) const override
    {
        vsg::Object::write(output);

        output.write("alive", alive);
        output.write("frameCount", frameStamp->frameCount);
        output.write("lookAt.eye", lookAt->eye);
        output.write("lookAt.center", lookAt->center);
        output.write("lookAt.up", lookAt->up);
    }
};

}

// Provide the means for the vsg::type_name<class> to get the human readable class name.
EVSG_type_name(cluster::ViewerData);

// Register the ProjectorScene::create() method with vsg::ObjectFactory::instance() so it can be used for creating objects during reading.
vsg::RegisterWithObjectFactoryProxy<cluster::ViewerData> s_Register_ViewerData;


enum ViewerMode
{
    STAND_ALONE,
    SERVER,
    CLIENT
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgcluster";

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    auto pointOfInterest = arguments.value(vsg::dvec3(0.0, 0.0, std::numeric_limits<double>::max()), "--poi");
    auto horizonMountainHeight = arguments.value(0.0, "--hmh");

    // network settings.
    auto portNumber = arguments.value<uint16_t>(9000, "--port");
    auto ifrName = arguments.value(std::string(), "--ifr-name");
    auto hostName = arguments.value(std::string(), "--host");

    ViewerMode viewerMode = STAND_ALONE;
    if (arguments.read({"-s", "--serve"})) viewerMode = SERVER;
    if (arguments.read({"-c", "--client"})) viewerMode = CLIENT;

    if (arguments.read("--ifr-names"))
    {
        auto ifr_names = listNetworkConnections();
        for (auto& name : ifr_names)
        {
            std::cout << name << std::endl;
        }
        return 0;
    }

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    std::cout << "portNumber = " << portNumber << std::endl;
    std::cout << "ifrName = " << ifrName << std::endl;
    std::cout << "hostName = " << hostName << std::endl;
    std::cout << "viewerMode = " << viewerMode << std::endl;

    auto bc = Broadcaster::create_if(viewerMode == SERVER, portNumber, ifrName);
    auto rc = Receiver::create_if(viewerMode == CLIENT, portNumber);

    std::cout << "bc = " << bc << std::endl;
    std::cout << "rc = " << rc << std::endl;

    auto scene = vsg::Group::create();
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel;

    if (argc > 1)
    {
        vsg::Path filename = arguments[1];
        auto model = vsg::read_cast<vsg::Node>(filename, options);
        if (model)
        {
            scene->addChild(model);
            ellipsoidModel = model->getObject<vsg::EllipsoidModel>("EllipsoidModel");
        }
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    vsg::ref_ptr<vsg::LookAt> lookAt;

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scene->accept(computeBounds);
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

    if (pointOfInterest[2] != std::numeric_limits<double>::max())
    {
        if (ellipsoidModel)
        {
            auto ecef = ellipsoidModel->convertLatLongAltitudeToECEF({pointOfInterest[0], pointOfInterest[1], 0.0});
            auto ecef_normal = vsg::normalize(ecef);

            vsg::dvec3 centre = ecef;
            vsg::dvec3 eye = centre + ecef_normal * pointOfInterest[2];
            vsg::dvec3 up = vsg::normalize(vsg::cross(ecef_normal, vsg::cross(vsg::dvec3(0.0, 0.0, 1.0), ecef_normal)));

            // set up the camera
            lookAt = vsg::LookAt::create(eye, centre, up);
        }
        else
        {
            vsg::dvec3 eye = pointOfInterest;
            vsg::dvec3 centre = eye - vsg::dvec3(0.0, -radius * 3.5, 0.0);
            vsg::dvec3 up = vsg::dvec3(0.0, 0.0, 1.0);

            // set up the camera
            lookAt = vsg::LookAt::create(eye, centre, up);
        }
    }
    else
    {
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;

        // set up the camera
        lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    }

    double nearFarRatio = 0.001;
    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (ellipsoidModel)
    {
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

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

    std::vector<uint64_t> buffer(1);
    uint32_t buffer_size = buffer.size() * sizeof(decltype(buffer)::value_type);

    std::cout << "buffer_size = " << buffer_size << std::endl;

    PacketBroadcaster broadcaster;
    broadcaster.broadcaster = bc;

    PacketReceiver receiver;
    receiver.receiver = rc;

    auto viewerData = cluster::ViewerData::create();
    viewerData->frameStamp = viewer->getFrameStamp();
    viewerData->lookAt = lookAt;

    // rendering main loop
    while (viewer->advanceToNextFrame() && (!viewerData || viewerData->alive))
    {
        if (bc)
        {
            viewerData->frameStamp = viewer->getFrameStamp();
            viewerData->lookAt = lookAt;

            broadcaster.broadcast(viewer->getFrameStamp()->frameCount, viewerData);
        }

        if (rc)
        {
            //unsigned int size = rc->recieve(buffer.data(), buffer_size);
            //std::cout << "recieved size = " << size << std::endl;
            auto object = receiver.receive();
            viewerData = object.cast<cluster::ViewerData>();
            if (viewerData)
            {
                std::cout<<"recieved viewerData "<<viewerData->alive<<std::endl;

                lookAt->eye = viewerData->lookAt->eye;
                lookAt->center = viewerData->lookAt->center;
                lookAt->up = viewerData->lookAt->up;
            }
            else
            {
                std::cout<<"recieved "<<object<<std::endl;
            }
        }

        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    if (bc)
    {
        viewerData->alive = false;

        broadcaster.broadcast(viewer->getFrameStamp()->frameCount, viewerData);

        // vsg::write(viewerData, "test.vsgt");
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
