// Spot light shadows on PBR materials: run as is (no shadow), then with --fix.
#include <vsg/all.h>

#include <iostream>
#include <string>

// Flip the inverted cone check in the pbr fragment shader (phong already uses '<').
bool fixSpotShadowCone(vsg::ShaderSet& shaderSet)
{
    const std::string from = "if (lightDirection_cosOuterAngle.w > dot_lightdirection)";
    const std::string to = "if (lightDirection_cosOuterAngle.w < dot_lightdirection)";
    for (auto& stage : shaderSet.stages)
    {
        if (stage->stage != VK_SHADER_STAGE_FRAGMENT_BIT) continue;
        auto& source = stage->module->source;
        auto pos = source.find(from);
        if (pos == std::string::npos) return false;
        source.replace(pos, from.size(), to);
        stage->module->code.clear(); // recompile from the edited source
        shaderSet.variants.clear();
        return true;
    }
    return false;
}

vsg::ref_ptr<vsg::Node> createSpotLightScene(bool fix, bool pbr)
{
    auto builder = vsg::Builder::create();

    if (pbr)
    {
        auto shaderSet = vsg::createPhysicsBasedRenderingShaderSet();
        shaderSet->defaultShaderHints = vsg::ShaderCompileSettings::create();
        shaderSet->defaultShaderHints->defines.insert("VSG_SHADOWS_HARD");
        if (fix && !fixSpotShadowCone(*shaderSet)) std::cout << "pbr shader line not found" << std::endl;

        builder->shaderSet = shaderSet;
    }

    auto scene = vsg::Group::create();

    // ground and a box, both PBR
    vsg::GeometryInfo ground;
    ground.position.set(0.0f, 0.0f, -0.1f);
    ground.dx.set(20.0f, 0.0f, 0.0f);
    ground.dy.set(0.0f, 20.0f, 0.0f);
    ground.dz.set(0.0f, 0.0f, 0.2f);
    scene->addChild(builder->createBox(ground));

    vsg::GeometryInfo box;
    box.position.set(0.0f, 0.0f, 1.0f);
    box.dx.set(2.0f, 0.0f, 0.0f);
    box.dy.set(0.0f, 2.0f, 0.0f);
    box.dz.set(0.0f, 0.0f, 7.0f);
    scene->addChild(builder->createBox(box));

    auto ambient = vsg::AmbientLight::create();
    ambient->intensity = 0.05f;
    scene->addChild(ambient);

    // spot light placed by a transform, above and to the side of the box, pointing down
    auto spot = vsg::SpotLight::create();
    spot->position.set(0.0, 0.0, 0.0);
    spot->direction.set(0.0, 0.0, -1.0);
    spot->innerAngle = vsg::radians(30.0);
    spot->outerAngle = vsg::radians(45.0);
    spot->intensity = 100.0f;
    spot->shadowSettings = vsg::HardShadows::create(1);
    auto transform = vsg::MatrixTransform::create(vsg::translate(2.0, -1.0, 8.0));
    transform->addChild(spot);
    scene->addChild(transform);

    return scene;
}

vsg::ref_ptr<vsg::Node> createTransformScene(bool pbr)
{
    auto builder = vsg::Builder::create();

    if (pbr)
    {
        auto shaderSet = vsg::createPhysicsBasedRenderingShaderSet();
        shaderSet->defaultShaderHints = vsg::ShaderCompileSettings::create();
        shaderSet->defaultShaderHints->defines.insert("VSG_SHADOWS_HARD");

        builder->shaderSet = shaderSet;
    }

    auto scene = vsg::Group::create();

    // ground and a box, both PBR
    vsg::GeometryInfo ground;
    ground.position.set(0.0f, 0.0f, -0.1f);
    ground.dx.set(20.0f, 0.0f, 0.0f);
    ground.dy.set(0.0f, 20.0f, 0.0f);
    ground.dz.set(0.0f, 0.0f, 0.2f);
    scene->addChild(builder->createBox(ground));

    vsg::GeometryInfo box;
    box.position.set(0.0f, 0.0f, 1.0f);
    box.dx.set(2.0f, 0.0f, 0.0f);
    box.dy.set(0.0f, 2.0f, 0.0f);
    box.dz.set(0.0f, 0.0f, 7.0f);
    scene->addChild(builder->createBox(box));

    auto sun = vsg::DirectionalLight::create();
    sun->intensity = 2.0f;
    sun->shadowSettings = vsg::HardShadows::create(1);

    const auto d = vsg::normalize(vsg::dvec3(-0.35, -0.8, 0.45));

    // A) light at the root, direction in world space: correct shadow under the box
    sun->direction = d;
    scene->addChild(sun);

    // B) same light under a rotated transform, shining along its local -Z:
    //    the whole scene is in shadow
    sun->direction = vsg::dvec3(0.0, 0.0, -1.0);
    auto transform = vsg::MatrixTransform::create(
        vsg::translate(0.0, 20.0, 0.0) * vsg::rotate(vsg::dquat(vsg::dvec3(0.0, 0.0, -1.0), d)));
    transform->addChild(sun);
    scene->addChild(transform);

    // C) translation-only transform, direction in world space: correct again
    sun->direction = d;
    auto translated = vsg::MatrixTransform::create(vsg::translate(0.0, 20.0, 0.0));
    translated->addChild(sun);
    scene->addChild(translated);

    return scene;
}

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);
    bool fix = arguments.read("--fix");
    bool pbr = !arguments.read("--phong");
    bool transform = arguments.read({"-t", "--transform"});

    vsg::ref_ptr<vsg::Node> scene;
    if (transform)
    {
        scene = createTransformScene(pbr);
    }
    else
    {
        scene = createSpotLightScene(fix, pbr);
    }

    if (!scene) return 1;

    auto window = vsg::Window::create(vsg::WindowTraits::create(1024, 768, "spot shadow"));
    auto lookAt = vsg::LookAt::create(vsg::dvec3(0.0, -14.0, 10.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
    auto perspective = vsg::Perspective::create(45.0, 1024.0 / 768.0, 0.1, 100.0);
    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer), vsg::Trackball::create(camera)});
    viewer->assignRecordAndSubmitTaskAndPresentation({vsg::createCommandGraphForView(window, camera, scene)});
    viewer->compile();

    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();
    }
    return 0;
}
