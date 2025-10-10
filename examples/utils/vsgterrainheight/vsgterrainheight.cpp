#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>
#include <optional>
#include <random>

class IntersectionHandler : public vsg::Inherit<vsg::Object, IntersectionHandler>
{
public:
    vsg::ref_ptr<vsg::Group> scenegraph;
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel;
    bool verbose = false;
    vsg::ref_ptr<vsg::LineSegmentIntersector> intersector;

    IntersectionHandler(vsg::ref_ptr<vsg::Group> in_scenegraph, vsg::ref_ptr<vsg::EllipsoidModel> in_ellipsoidModel) :
        scenegraph(in_scenegraph),
        ellipsoidModel(in_ellipsoidModel),
        intersector(nullptr)
    {
    }

    std::optional<vsg::dvec3> intersection_LineSegmentIntersector(const vsg::dvec3& start, const vsg::dvec3& end)
    {
        if (!intersector)
        {
            intersector = vsg::LineSegmentIntersector::create(start, end);
        }
        else
        {
            intersector->reset(start, end);
        }

        auto before_intersection = vsg::clock::now();

        scenegraph->accept(*intersector);

        auto after_intersection = vsg::clock::now();

        if (verbose)
        {
            std::cout << "\nintersection_LineSegmentIntersector(" << start << ", " << end << ") " << intersector->intersections.size() << ")";
            std::cout << "time = " << std::chrono::duration<double, std::chrono::milliseconds::period>(after_intersection - before_intersection).count() << "ms" << std::endl;
        }

        if (intersector->intersections.empty()) return std::nullopt;

        // sort the intersections front to back
        std::sort(intersector->intersections.begin(), intersector->intersections.end(), [](auto& lhs, auto& rhs) { return lhs->ratio < rhs->ratio; });

        return intersector->intersections.front()->worldIntersection;
    }
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create(arguments);

    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

    auto pointOfInterest = arguments.value(vsg::dvec3(0.0, 0.0, std::numeric_limits<double>::max()), "--poi");
    auto horizonMountainHeight = arguments.value(0.0, "--hmh");

    auto queryLocationCount = arguments.value(100000, "-n");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto scene = vsg::Group::create();
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel;

    auto intersectionOptimizeVisitor = vsg::IntersectionOptimizeVisitor::create();
    for (auto& readerWriter : options->readerWriters)
    {
        readerWriter = vsg::ApplyVisitorReader::create(readerWriter, intersectionOptimizeVisitor);
    }

    if (argc > 1)
    {
        vsg::Path filename = arguments[1];
        auto model = vsg::read_cast<vsg::Node>(filename, options);
        if (model)
        {
            scene->addChild(model);
            ellipsoidModel = model->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
        }
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
            lookAt = vsg::LookAt::create(eye, ecef, up);
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
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        vsg::dvec3 eye = centre + vsg::dvec3(0.0, -radius * 3.5, 0.0);
        if (ellipsoidModel)
        {
            auto lla = ellipsoidModel->convertECEFToLatLongAltitude(centre);
            lla.z += radius;
            eye = ellipsoidModel->convertLatLongAltitudeToECEF(lla);
        }
        lookAt = vsg::LookAt::create(eye, centre, vsg::dvec3(0.0, 0.0, 1.0));
    }

    auto intersectionHandler = IntersectionHandler::create(scene, ellipsoidModel);

    // default-construct to get the default seed and therefore deterministic values;
    std::mt19937 randomEngine;
    std::uniform_real_distribution randomDist(0.3, 0.7);
    std::vector<vsg::dvec3> queryLocations;
    queryLocations.reserve(queryLocationCount);
    vsg::dvec3 down(0.0, 0.0, computeBounds.bounds.min.z - computeBounds.bounds.max.z);
    if (ellipsoidModel)
    {
        // we need the bounds in the local geodetic coordinate system
        vsg::dvec3 origin = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        auto originLLA = ellipsoidModel->convertECEFToLatLongAltitude(origin);
        auto ecefToGeodetic = ellipsoidModel->computeWorldToLocalTransform(originLLA);
        auto sceneGeodetic = vsg::MatrixTransform::create(ecefToGeodetic);
        sceneGeodetic->addChild(scene);
        vsg::ComputeBounds localBounds;
        sceneGeodetic->accept(localBounds);
        down = normalize(origin) * (localBounds.bounds.min.z - localBounds.bounds.max.z);

        auto geodeticToEcef = ellipsoidModel->computeLocalToWorldTransform(originLLA);

        for (size_t i = 0; i < queryLocationCount; ++i)
        {
            vsg::dvec3 localPoint{
                localBounds.bounds.min.x + randomDist(randomEngine) * (localBounds.bounds.max.x - localBounds.bounds.min.x),
                localBounds.bounds.min.y + randomDist(randomEngine) * (localBounds.bounds.max.y - localBounds.bounds.min.y),
                localBounds.bounds.max.z};
            queryLocations.emplace_back(geodeticToEcef * localPoint);
        }
    }
    else
    {
        for (size_t i = 0; i < queryLocationCount; ++i)
        {
            queryLocations.emplace_back(
                computeBounds.bounds.min.x + randomDist(randomEngine) * (computeBounds.bounds.max.x - computeBounds.bounds.min.x),
                computeBounds.bounds.min.y + randomDist(randomEngine) * (computeBounds.bounds.max.y - computeBounds.bounds.min.y),
                computeBounds.bounds.max.z);
        }
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

    auto commandGraph = createCommandGraphForView(window, camera, scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

    viewer->compile();

    int iterations = 0;
    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();

        size_t hits = 0;

        for (const auto& location : queryLocations)
        {
            if (intersectionHandler->intersection_LineSegmentIntersector(location, location + down).has_value())
                ++hits;
        }
        std::cout << "Hits: " << hits << ", misses: " << queryLocationCount - hits << std::endl;

        ++iterations;
        if (iterations == 10)
            viewer->close();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
