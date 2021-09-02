#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>


// use a static handle that is initialized once at start up to avoid multi-threaded issues associated with calling std::locale::classic().
struct DataBlocks : public vsg::Inherit<vsg::Object, DataBlocks>
{
    uint32_t total_rows = 0;
    std::vector<vsg::ref_ptr<vsg::floatArray2D>> blocks;
};

vsg::ref_ptr<DataBlocks> readDataBlocks(std::istream& fin)
{
    if (!fin) return {};

    unsigned int maxWidth = 1024;
    unsigned int maxBlockHeight = 1024;

    std::vector<float> values(maxWidth);

    uint32_t numValues = 0;
    while(fin && (numValues = vsg::read_line(fin, values.data(), maxWidth)) == 0) {}

    if (numValues == 0) return {};


    auto dataBlocks = DataBlocks::create();

    auto current_block = vsg::floatArray2D::create(numValues, maxBlockHeight);

    dataBlocks->blocks.push_back(current_block);

    unsigned int current_row = 0;

    // copy across already read first row
    for(uint32_t c = 0; c < numValues; ++c)
    {
        current_block->set(c, current_row, values[c]);
    }

    ++current_row;
    ++(dataBlocks->total_rows);

    while(fin)
    {
        if (current_row >= current_block->height())
        {
            current_block = vsg::floatArray2D::create(numValues, maxBlockHeight);
            dataBlocks->blocks.push_back(current_block);

            current_row = 0;
        }
        if (vsg::read_line(fin, &(current_block->at(0, current_row)), numValues) == numValues)
        {
            ++current_row;
            ++(dataBlocks->total_rows);
        }
    }

    return dataBlocks;
}

struct FormatLayout
{
    int vertex = 0;
    int rgb = -1;
    int normal = -1;
};

vsg::DataList combineDataBlocks(vsg::ref_ptr<DataBlocks> dataBlocks, FormatLayout formatLayout)
{
    std::cout<<"blocks.size() = "<<dataBlocks->blocks.size()<<", numVertices = "<<dataBlocks->total_rows<<std::endl;

    vsg::DataList arrays;

    uint32_t numVertices = dataBlocks->total_rows;

    if (formatLayout.vertex >= 0)
    {
        auto vertices = vsg::vec3Array::create(numVertices);
        arrays.push_back(vertices);

        auto itr = vertices->begin();

        for(auto& block : dataBlocks->blocks)
        {
            auto proxy_vertices = vsg::vec3Array::create(block, 4 * formatLayout.vertex, 4 * block->width(), block->height());
            for(auto& v : *proxy_vertices) *(itr++) = v;
        }

        std::cout<<"vertices = "<<vertices->size()<<std::endl;
    }

    if (formatLayout.normal >= 0)
    {
        auto normals = vsg::vec3Array::create(numVertices);
        arrays.push_back(normals);

        auto itr = normals->begin();

        for(auto& block : dataBlocks->blocks)
        {
            auto proxy_vertices = vsg::vec3Array::create(block, 4 * formatLayout.normal, 4 * block->width(), block->height());
            for(auto& v : *proxy_vertices) *(itr++) = v;
        }

        std::cout<<"normals = "<<normals->size()<<std::endl;
    }


    if (formatLayout.rgb >= 0)
    {
        auto colors = vsg::ubvec4Array::create(numVertices);
        arrays.push_back(colors);

        auto itr = colors->begin();

        for(auto& block : dataBlocks->blocks)
        {
            auto proxy_colors = vsg::vec3Array::create(block, 4 * formatLayout.rgb, 4 * block->width(), block->height());
            for(auto& c : *proxy_colors)
            {
                *(itr++) = vsg::ubvec4(static_cast<uint8_t>(c.r), static_cast<uint8_t>(c.g), static_cast<uint8_t>(c.b), 255);
            }
        }

        std::cout<<"colours = "<<colors->size()<<std::endl;
    }

    return arrays;
}

vsg::ref_ptr<vsg::Node> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options, FormatLayout formatLayout)
{
    vsg::Path filenameToUse = vsg::findFile(filename, options);
    if (filenameToUse.empty()) return {};

    std::ifstream fin(filenameToUse.c_str());
    if (!fin) return { };

    fin.imbue(std::locale::classic());

    auto dataBlocks = readDataBlocks(fin);
    if (!dataBlocks || dataBlocks->total_rows==0) return {};

    auto arrays = combineDataBlocks(dataBlocks, formatLayout);
    if (arrays.empty()) return {};

    auto bindVertexBuffers = vsg::BindVertexBuffers::create();
    bindVertexBuffers->arrays = arrays;

    auto commands = vsg::Commands::create();
    commands->addChild(bindVertexBuffers);
    commands->addChild(vsg::Draw::create(dataBlocks->total_rows, 1, 0, 0));

    return commands;
}


int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->objectCache = vsg::ObjectCache::create();

#ifdef vsgXchange_allls
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgpoints";

    auto builder = vsg::Builder::create();
    builder->options = options;

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});

    vsg::GeometryInfo geomInfo;
    geomInfo.dx.set(1.0f, 0.0f, 0.0f);
    geomInfo.dy.set(0.0f, 1.0f, 0.0f);
    geomInfo.dz.set(0.0f, 0.0f, 1.0f);

    vsg::StateInfo stateInfo;

    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    auto numFrames = arguments.value(-1, "-f");
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--double-buffer")) windowTraits->swapchainPreferences.imageCount = 2;
    if (arguments.read("--triple-buffer")) windowTraits->swapchainPreferences.imageCount = 3; // defaul
    if (arguments.read("-t"))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
    }

    auto outputFilename = arguments.value<std::string>("", "-o");

    FormatLayout formatLayout;

    arguments.read("--xyx", formatLayout.vertex);
    arguments.read("--rgb", formatLayout.rgb);
    arguments.read("--normal", formatLayout.normal);

    auto scene = vsg::Group::create();

    for(int i=1; i<argc; ++i)
    {
        std::string filename = argv[i];
        auto ext = vsg::lowerCaseFileExtension(filename);
        if (ext != "asc" && ext != "3dc")
        {
            formatLayout.vertex = 0;
            formatLayout.rgb = 3;
            formatLayout.normal = 6;
        }

        auto model = read(filename, options, formatLayout);
        std::cout<<"model = "<<filename<<" "<<model<<std::endl;
        if (model) scene->addChild(model);
    }

    if (scene->children.empty())
    {
        std::cout<<"No scene graph created."<<std::endl;
        return 1;
    }

    // write out scene if required
    if (!outputFilename.empty())
    {
        vsg::write(scene, outputFilename, options);
        return 0;
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

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6 * 10.0;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    double nearFarRatio = 0.001;
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.0);

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // set up the compilation support in builder to allow us to interactively create and compile subgraphs from wtihin the IntersectionHandler
    // builder->setup(window, camera->viewportState);

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera));

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
        std::cout<<"Average frame rate = "<<(numFramesCompleted / duration)<<std::endl;
    }

    return 0;
}
