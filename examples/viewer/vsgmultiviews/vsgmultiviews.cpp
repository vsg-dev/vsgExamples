#include <vsg/all.h>

#include <vsgImGui/RenderImGui.h>
#include <vsgImGui/SendEventsToImGui.h>
#include <vsgImGui/imgui.h>

#include <chrono>
#include <iostream>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

struct Params : public vsg::Inherit<vsg::Object, Params>
{
    bool showGui = true; // you can toggle this with your own EventHandler and key
    bool showDemoWindow = false;
    bool showSecondWindow = false;
    float clearColor[3]{0.2f, 0.2f, 0.4f}; // Unfortunately, this doesn't change dynamically in vsg
    uint32_t counter = 0;
    float dist = 0.f;
};

class MyGuiComponent
{
public:
    MyGuiComponent(vsg::ref_ptr<Params> params) :
        _params(params)
    {
    }

    // Example here taken from the Dear imgui comments (mostly)
    bool operator()()
    {
        bool visibleComponents = false;

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        if (_params->showGui)
        {
            ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

            ImGui::Text("Some useful message here.");                 // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &_params->showDemoWindow); // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &_params->showSecondWindow);
            ImGui::SliderFloat("float", &_params->dist, 0.0f, 1.0f);        // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&_params->clearColor); // Edit 3 floats representing a color

            if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
                _params->counter++;

            ImGui::SameLine();
            ImGui::Text("counter = %d", _params->counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();

            visibleComponents = true;
        }

        // 3. Show another simple window.
        if (_params->showSecondWindow)
        {
            ImGui::Begin("Another Window", &_params->showSecondWindow); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                _params->showSecondWindow = false;
            ImGui::End();

            visibleComponents = true;
        }

        if (_params->showDemoWindow)
        {
            ImGui::ShowDemoWindow(&_params->showDemoWindow);

            visibleComponents = true;
        }

        return visibleComponents;
    }

private:
    vsg::ref_ptr<Params> _params;
};

vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* scenegraph, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0),
                                      centre, vsg::dvec3(0.0, 0.0, 1.0));

    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(width) / static_cast<double>(height),
                                                nearFarRatio * radius, radius * 4.5);

    auto viewportstate = vsg::ViewportState::create(x, y, width, height);

    return vsg::Camera::create(perspective, lookAt, viewportstate);
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "Multiple Views";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    windowTraits->height = 600;
    windowTraits->width = 800;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }

    bool separateRenderGraph = arguments.read("-s");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    auto options = vsg::Options::create();
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    vsg::ref_ptr<vsg::Node> scenegraph;
    vsg::ref_ptr<vsg::Node> scenegraph2;
    if (argc > 1)
    {
        vsg::Path filename = arguments[1];
        scenegraph = vsg::read_cast<vsg::Node>(filename, options);
        scenegraph2 = scenegraph;
    }

    if (argc > 2)
    {
        vsg::Path filename = arguments[2];
        scenegraph2 = vsg::read_cast<vsg::Node>(filename, options);
    }

    if (!scenegraph || !scenegraph2)
    {
        std::cout << "Please specify a valid model on command line" << std::endl;
        return 1;
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

    uint32_t width = window->extent2D().width;
    uint32_t height = window->extent2D().height;

    // create the vsg::RenderGraph and associated vsg::View
    auto main_camera = createCameraForScene(scenegraph, 0, 0, width, height);
    auto main_view = vsg::View::create(main_camera, scenegraph);

    // create an RenderinGraph to add an secondary vsg::View on the top right part of the window.
    auto secondary_camera = createCameraForScene(scenegraph2, (width * 3) / 4, 0, width / 4, height / 4);
    auto secondary_view = vsg::View::create(secondary_camera, scenegraph2);

    auto sw = vsg::Switch::create();
    auto swImGUI = vsg::Switch::create();

    if (separateRenderGraph)
    {
        std::cout << "Using a RenderGraph per View" << std::endl;
        auto main_RenderGraph = vsg::RenderGraph::create(window, main_view);
        auto secondary_RenderGraph = vsg::RenderGraph::create(window, secondary_view);
        auto imgui_RenderGraph = vsg::RenderGraph::create(window);
        secondary_RenderGraph->clearValues[0].color = {0.2f, 0.2f, 0.2f, 1.0f};
        imgui_RenderGraph->clearValues[0].color = {0.3f, 0.3f, 0.3f, 1.0f};

        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(main_RenderGraph);
        commandGraph->addChild(sw);
        commandGraph->addChild(imgui_RenderGraph);

        sw->addChild(true, secondary_RenderGraph);

        auto params = Params::create();
        swImGUI->addChild(true, vsgImGui::RenderImGui::create(window, MyGuiComponent(params)));

        imgui_RenderGraph->addChild(swImGUI);

        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
    }
    else
    {
        std::cout << "Using a single RenderGraph, with both Views separated by a ClearAttachemnts" << std::endl;
        auto renderGraph = vsg::RenderGraph::create(window);

        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(renderGraph);

        renderGraph->addChild(main_view);

        // clear the depth buffer before view2 gets rendered
        VkClearAttachment color_attachment{VK_IMAGE_ASPECT_COLOR_BIT, 0, VkClearValue{0.2f, 0.2f, 0.2f, 1.0f}};
        VkClearAttachment depth_attachment{VK_IMAGE_ASPECT_DEPTH_BIT, 1, VkClearValue{1.0f, 0.0f}};
        VkClearRect rect{secondary_camera->getRenderArea(), 0, 1};
        auto clearAttachments = vsg::ClearAttachments::create(vsg::ClearAttachments::Attachments{color_attachment, depth_attachment}, vsg::ClearAttachments::Rects{rect, rect});

        sw->addChild(true, clearAttachments);
        sw->addChild(true, secondary_view);

        auto params = Params::create();
        swImGUI->addChild(true, vsgImGui::RenderImGui::create(window, MyGuiComponent(params)));

        renderGraph->addChild(sw);
        renderGraph->addChild(swImGUI);

        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
    }

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // Add the ImGui event handler first to handle events early
    viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());

    // add event handlers, in the order we wish event to be handled.
    viewer->addEventHandler(vsg::Trackball::create(secondary_camera));
    viewer->addEventHandler(vsg::Trackball::create(main_camera));

    viewer->compile();

    int count = 0;
    int countIMGUI = 0;

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        // toggle the Switch decorating the secondary view
        sw->setAllChildren(((count / 60) % 2) == 0);

        // toggle the Switch decorating the secondary view
        swImGUI->setAllChildren(((countIMGUI / 120) % 2) == 0);

        //++countIMGUI;

        ++count;

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
