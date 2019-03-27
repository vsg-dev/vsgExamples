#include <vsg/all.h>
#include <iostream>

#include "Text.h"

class KeyboardInput : public vsg::Inherit<vsg::Visitor, KeyboardInput>
{
public:
    KeyboardInput(vsg::ref_ptr<vsg::Viewer> viewer, vsg::ref_ptr<vsg::TextGroup> textGroup, vsg::Paths searchPaths) :
        _viewer(viewer),
        _textGroup(textGroup),
        _shouldRecompile(false),
        _shouldReleaseViewport(false)
    {
        vsg::ref_ptr<vsg::Window> window(_viewer->windows()[0]);

        auto width = window->extent2D().width;
        auto height = window->extent2D().height;

        auto font = vsg::Font::create("roboto", searchPaths);

        _keyboardInputText = vsg::Text::create(font, _textGroup);
        _textGroup->addChild(_keyboardInputText);

        _keyboardInputText->setPosition(vsg::vec3(-(width*0.5f), (height*0.5f) - 50, 0.0f));
        _keyboardInputText->setText("Cheese!");
    }

    void apply(vsg::ConfigureWindowEvent& configure)
    {
        auto width = configure.width;
        auto height = configure.height;

        vsg::ref_ptr<vsg::Window> window(configure.window);
        vsg::ref_ptr<vsg::GraphicsStage> graphicsStage = vsg::ref_ptr<vsg::GraphicsStage>(dynamic_cast<vsg::GraphicsStage*>(window->stages()[0].get()));
        vsg::Camera* camera = graphicsStage->_camera;

        vsg::ref_ptr<vsg::Orthographic> orthographic(new vsg::Orthographic(-(width*0.5f), (width*0.5f), -(height*0.5f), (height*0.5f), 0.1, 10.0));
        
        camera->setProjectionMatrix(orthographic);
        _keyboardInputText->setPosition(vsg::vec3(-(width*0.5f), (height*0.5f) - 50, 0.0f));

        //rand color
        float randcolor = (rand() % 10) / 10.0f;
        window->clearColor().float32[0] = randcolor;

        _shouldRecompile = true;
        _shouldReleaseViewport = true;
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        // ignore modifier keys
        if(keyPress.keyBase >= vsg::KeySymbol::KEY_Shift_L && keyPress.keyBase <= vsg::KeySymbol::KEY_Hyper_R) return;

        std::string textstr = _keyboardInputText->getText();

        if (keyPress.keyBase == vsg::KeySymbol::KEY_BackSpace)
        {
            if(textstr.size() == 0) return;
            _keyboardInputText->setText(textstr.substr(0, textstr.size() - 1));
            _shouldRecompile = true;
            return;
        }

        if (keyPress.keyBase == vsg::KeySymbol::KEY_Return)
        {
            _keyboardInputText->setText(textstr + '\n');
            _shouldRecompile = true;
            return;
        }

        textstr += keyPress.keyModified;
        _keyboardInputText->setText(textstr);
        _shouldRecompile = true;
    }

    const bool& shouldRecompile() const { return _shouldRecompile; }
    
    void reset()
    { 
        _shouldRecompile = false;
        // hack, release the graphics pipeline implementation to allow new viewport to recompile
        if(_shouldReleaseViewport)
        {
            _textGroup->_bindGraphicsPipeline->getPipeline()->release();
            _shouldReleaseViewport = false;
        }
    }

protected:
    vsg::ref_ptr<vsg::Viewer> _viewer;
    vsg::ref_ptr<vsg::TextGroup> _textGroup;
    vsg::ref_ptr<vsg::Text> _keyboardInputText;

    // flag used to force recompile
    bool _shouldRecompile;
    bool _shouldReleaseViewport;
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto [width, height] = arguments.value(std::pair<uint32_t, uint32_t>(800, 600), {"--window", "-w"});
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // create StateGroup as the root of the scene
    auto scenegraph = vsg::StateGroup::create();

    // transform
    auto transform = vsg::MatrixTransform::create(); // there must be a transform, guessing to populate the push constant
    scenegraph->addChild(transform);

    // create text group
    auto textgroup = vsg::TextGroup::create(searchPaths);
    transform->addChild(textgroup);

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(width, height, debugLayer, apiDumpLayer));
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // camera related details
    auto viewport = vsg::ViewportState::create(VkExtent2D{width, height});
    //vsg::ref_ptr<vsg::Perspective> perspective(new vsg::Perspective(60.0, static_cast<double>(width) / static_cast<double>(height), 0.1, 10.0));
    vsg::ref_ptr<vsg::Orthographic> orthographic(new vsg::Orthographic(-(width*0.5f), (width*0.5f), -(height*0.5f), (height*0.5f), 0.1, 10.0));
    vsg::ref_ptr<vsg::LookAt> lookAt(new vsg::LookAt(vsg::dvec3(0.0, 0.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0)));
    vsg::ref_ptr<vsg::Camera> camera(new vsg::Camera(orthographic, lookAt, viewport));

    // add a GraphicsStage to the Window to do dispatch of the command graph to the commnad buffer(s)
    window->addStage(vsg::GraphicsStage::create(scenegraph, camera));

    // keyboard input for demo
    vsg::ref_ptr<KeyboardInput> keyboardInput = KeyboardInput::create(viewer, textgroup, searchPaths);

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer), keyboardInput});

    // compile the Vulkan objects
    viewer->compile();

    // main frame loop
    while (viewer->active())
    {
        // poll events and advance frame counters
        viewer->advance();

        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        if (viewer->aquireNextFrame())
        {
            // animate the transform
            float time = std::chrono::duration<float, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();
            //transform->setMatrix(vsg::rotate(time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f)));

            if (keyboardInput->shouldRecompile())
            {
                keyboardInput->reset(); // this releases the graphicspipline implementation
                viewer->compile();
            }

            viewer->populateNextFrame();

            viewer->submitNextFrame();
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
