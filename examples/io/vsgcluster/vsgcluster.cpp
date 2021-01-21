#include <vsg/all.h>

#ifdef USE_VSGXCHANGE
#    include <vsgXchange/ReaderWriter_all.h>
#endif

#include <iostream>

#include "Broadcaster.h"
#include "Receiver.h"
#include "Packet.h"


void test()
{
    std::cout<<"void test()"<<std::endl;

    Packets packets;

    packets.packets[0] = std::unique_ptr<Packet>(new Packet);

    std::cout<<"middle"<<std::endl;

    packets.packets[1] = std::unique_ptr<Packet>(new Packet);

    packets.clear();

    packets.copy(std::string("ABCDEFGHIJKLMNOPQRSTUVCW0123"));

    std::cout<<"assemble = ["<<packets.assemble()<<"]"<<std::endl;

    packets.copy(std::string("23"));

    std::cout<<"assemble = ["<<packets.assemble()<<"]"<<std::endl;

    std::cout<<"end"<<std::endl;

}


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

    if (arguments.read("--test"))
    {
        test();
        return 0;
    }

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

#ifdef USE_VSGXCHANGE
    // add use of vsgXchange's support for reading and writing 3rd party file formats
    options->readerWriter = vsgXchange::ReaderWriter_all::create();
#endif

    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

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
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

    if (pointOfInterest[2] != std::numeric_limits<double>::max())
    {
        if (ellipsoidModel)
        {
            auto ecef = ellipsoidModel->convertLatLongHeightToECEF({vsg::radians(pointOfInterest[0]), vsg::radians(pointOfInterest[1]), 0.0});
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

    viewer->addEventHandler(vsg::Trackball::create(camera));

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

    //auto testmodel = vsg::Group::create();//scene;
    auto testmodel = scene;

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        if (bc)
        {
            uint64_t set = viewer->getFrameStamp()->frameCount;

            // bc->broadcast(buffer.data(), buffer_size);
            broadcaster.broadcast(set, testmodel);
        }

        if (rc)
        {
            //unsigned int size = rc->recieve(buffer.data(), buffer_size);
            //std::cout << "recieved size = " << size << std::endl;
            auto object = receiver.receive();
            std::cout<<"recieved "<<object<<std::endl;
        }

        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
