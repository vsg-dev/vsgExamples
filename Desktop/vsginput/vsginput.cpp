#include <vsg/all.h>

#include <iostream>
#include <iomanip>

#include "Text.h"

class KeyboardInput : public vsg::Inherit<vsg::Visitor, KeyboardInput>
{
public:
    KeyboardInput(vsg::ref_ptr<vsg::Viewer> viewer, vsg::ref_ptr<vsg::Group> root, vsg::Paths searchPaths) :
        _viewer(viewer),
        _root(root),
        _shouldRecompile(false)
    {
        vsg::ref_ptr<vsg::Window> window(_viewer->windows()[0]);

        auto width = window->extent2D().width;
        auto height = window->extent2D().height;

        vsg::ref_ptr<vsg::StateGroup> stategroup = vsg::StateGroup::create();
        root->addChild(stategroup);

        vsg::ref_ptr<vsg::TextGraphicsPipelineBuilder> textPipelineBuilder = vsg::TextGraphicsPipelineBuilder::create(searchPaths);
        stategroup->add(vsg::BindGraphicsPipeline::create(textPipelineBuilder->getGraphicsPipeline()));

        // any text attached below the font node will use it's atlas and lookup texture descriptor set (0)
        _font = vsg::Font::create(textPipelineBuilder->getGraphicsPipeline()->getPipelineLayout(), "roboto", searchPaths);
        stategroup->add(_font);

        _keyboardInputText = vsg::Text::create(_font, textPipelineBuilder->getGraphicsPipeline());
        stategroup->addChild(_keyboardInputText);

        _textGroup = vsg::TextGroup::create(_font, textPipelineBuilder->getGraphicsPipeline());
        stategroup->addChild(_textGroup);

        //
        _keyboardInputText->setFontHeight(50.0f);
        _keyboardInputText->setText("VulkanSceneGraph!");

        _keyboardInputText->setPosition(vsg::vec3(-(width*0.5f), (height*0.5f) - _keyboardInputText->getFontHeight(), 0.0f));
    }

    void apply(vsg::ConfigureWindowEvent& configure) override
    {
        auto width = configure.width;
        auto height = configure.height;

        vsg::ref_ptr<vsg::Window> window(configure.window);

        // auto orthographic = vsg::Orthographic::create(-(width*0.5f), (width*0.5f), -(height*0.5f), (height*0.5f), 0.1, 1000.0);
        // camera->setProjectionMatrix(orthographic);
        // _keyboardInputText->setPosition(vsg::vec3(-(width*0.5f), (height*0.5f) - _keyboardInputText->getFontHeight(), 0.0f));
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        // ignore modifier keys
        if(keyPress.keyBase >= vsg::KeySymbol::KEY_Shift_L && keyPress.keyBase <= vsg::KeySymbol::KEY_Hyper_R) return;

        std::string textstr = _keyboardInputText->getText();

        if(keyPress.keyBase < 32 || keyPress.keyBase > 126)
        {
            if (keyPress.keyBase == vsg::KeySymbol::KEY_BackSpace)
            {
                if(textstr.size() == 0) return;
                _keyboardInputText->setText(textstr.substr(0, textstr.size() - 1));
                _shouldRecompile = true;
            }
            else if (keyPress.keyBase == vsg::KeySymbol::KEY_Return)
            {
                _keyboardInputText->setText(textstr + '\n');
                _shouldRecompile = true;
            }
            return;
        }

        textstr += keyPress.keyModified;
        _keyboardInputText->setText(textstr);
        _shouldRecompile = true;
    }

    void apply(vsg::KeyReleaseEvent& keyPress) override
    {
        if (keyPress.keyBase == vsg::KeySymbol::KEY_Up)
        {
            _textGroup->setFontHeight(_textGroup->getFontHeight() * 1.1f);
        }
        else if (keyPress.keyBase == vsg::KeySymbol::KEY_Down)
        {
            _textGroup->setFontHeight(_textGroup->getFontHeight() * 0.9f);
        }
        else if (keyPress.keyBase == vsg::KeySymbol::KEY_Left)
        {
            _density = _density - 10;
            if (_density <= 0) _density = 10;
            randomFillCube(vsg::vec3(-400.0f, -400.0f, -800.0f), vsg::vec3(800.0f, 800.0f, 800.0f), _density, _textGroup->getFontHeight());
        }
        else if (keyPress.keyBase == vsg::KeySymbol::KEY_Right)
        {
            _density = _density + 10;
            if (_density > 100) _density = 100;
            randomFillCube(vsg::vec3(-400.0f, -400.0f, -800.0f), vsg::vec3(800.0f, 800.0f, 800.0f), _density, _textGroup->getFontHeight());
        }
        // Need to look into function keys
        else if (keyPress.keyBase == vsg::KeySymbol::KEY_0)
        {
            _textGroup->setBillboardAxis(vsg::vec3(0.0f, 0.0f, 0.0f));
        }
        else if (keyPress.keyBase == vsg::KeySymbol::KEY_1)
        {
            _textGroup->setBillboardAxis(vsg::vec3(1.0f,1.0f,1.0f));
        }
        else if (keyPress.keyBase == vsg::KeySymbol::KEY_2)
        {
            _textGroup->setBillboardAxis(vsg::vec3(0.0f, 1.0f, 1.0f));
        }
        else if (keyPress.keyBase == vsg::KeySymbol::KEY_3)
        {
            _textGroup->setBillboardAxis(vsg::vec3(1.0f, 0.0f, 1.0f));
        }
        else if (keyPress.keyBase == vsg::KeySymbol::KEY_4)
        {
            _textGroup->setBillboardAxis(vsg::vec3(1.0f, 1.0f, 0.0f));
        }
    }


    void randomFillCube(const vsg::vec3& offset, const vsg::vec3& size, uint32_t density, float fontHeight)
    {
        _textGroup->clear();
        _textGroup->setFontHeight(fontHeight);
        
        float invdensity = 1.0f / density;
        vsg::vec3 cellsize = size * invdensity;

        for (uint32_t z = 0; z < density; z++)
        {
            for (uint32_t y = 0; y < density; y++)
            {
                for (uint32_t x = 0; x < density; x++)
                {
                    vsg::vec3 point = offset + vsg::vec3(cellsize.x * x, cellsize.y * y, cellsize.z * z); // vsg::vec3(fmod((float)rand(), size.y), fmod((float)rand(), size.y), fmod((float)rand(), size.y));
                    std::ostringstream ss;
                    ss << "VSG";
                    _textGroup->addText(ss.str(), point);
                }
            }
        }

        std::cout << "compiling unique texts objects: " << _textGroup->getNumTexts() << std::endl;
        _textGroup->buildTextGraph();
        _shouldRecompile = true;
    }

    const bool& shouldRecompile() const { return _shouldRecompile; }
    
    void reset()
    { 
        _shouldRecompile = false;
    }

protected:
    vsg::ref_ptr<vsg::Viewer> _viewer;
    vsg::ref_ptr<vsg::Group> _root;
    vsg::ref_ptr<vsg::Font> _font;
    vsg::ref_ptr<vsg::Text> _keyboardInputText;

    vsg::ref_ptr<vsg::TextGroup> _textGroup;
    uint32_t _density = 0;

    // flag used to force recompile
    bool _shouldRecompile;
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto usePerspective = arguments.read({ "--perspective","-p" });
    auto [width, height] = arguments.value(std::pair<uint32_t, uint32_t>(800, 600), {"--window", "-w"});
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // create StateGroup as the root of the scene
    auto scenegraph = vsg::StateGroup::create();

    // transform
    auto transform = vsg::MatrixTransform::create(); // there must be a transform, guessing to populate the push constant
    scenegraph->addChild(transform);

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    vsg::ref_ptr<vsg::WindowTraits> traits = vsg::WindowTraits::create();
    traits->width = width;
    traits->height = height;
    traits->swapchainPreferences.presentMode = VkPresentModeKHR::VK_PRESENT_MODE_IMMEDIATE_KHR;

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(traits, debugLayer, apiDumpLayer));
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // camera related details
    auto viewport = vsg::ViewportState::create(VkExtent2D{width, height});

    vsg::ref_ptr<vsg::ProjectionMatrix> projection;
    vsg::ref_ptr<vsg::LookAt> lookAt;
    if (usePerspective)
    {
        projection = vsg::Perspective::create(60.0, static_cast<double>(width) / static_cast<double>(height), 0.1, 2000.0);
        lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 0.0, 100.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0));
    }
    else
    {
        projection = vsg::Orthographic::create(-(width*0.5f), (width*0.5f), -(height*0.5f), (height*0.5f), 0.1, 1000.0);
        lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 0.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0));
    }

    auto camera = vsg::Camera::create(projection, lookAt, viewport);

    // keyboard input for demo
    vsg::ref_ptr<KeyboardInput> keyboardInput = KeyboardInput::create(viewer, scenegraph, searchPaths);

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer), keyboardInput});

    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile the Vulkan objects
    viewer->compile();

    auto before = std::chrono::steady_clock::now();

    // main frame loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        if (keyboardInput->shouldRecompile())
        {
            keyboardInput->reset(); // this releases the graphicspipline implementation
            viewer->compile();
        }

        viewer->recordAndSubmit();

        viewer->present();
    }

    auto runtime = std::chrono::duration<double, std::chrono::seconds::period>(std::chrono::steady_clock::now() - before).count();
    std::cout << "avg fps: " << 1.0 / (runtime / (double)viewer->getFrameStamp()->frameCount) <<std::endl;

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
