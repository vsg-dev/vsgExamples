// Shadow acne on steep slopes with a directional light (VSG pbr ShaderSet).
//
// A procedural terrain with steep ridges around a flat area with a box, lit by a
// directional light with cascaded shadow maps. ViewDependentState only offers a
// constant depth bias (shadowMapBias); on slopes at a grazing angle to the light the
// surface shadows itself with fine hatching. --fix patches the pbr fragment shader to
// move the shadow lookup point along the normal (normal-offset bias).
//
//   shadowacne [--pcss | --soft r | --hard] [--sb bias] [--angleSubtended rad]
//              [--elevation deg] [--fix] [--distance m] [--frames n --screenshot file.ppm]

#include <vsg/all.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>

#ifdef SHADOWACNE_CAPTURE
#    include "capture.h"
#endif

namespace
{
    vsg::ref_ptr<vsg::ShaderSet> pbr_ShaderSet(vsg::ref_ptr<const vsg::Options> options)
    {
        vsg::info("Local pbr_ShaderSet(", options, ")");

        auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard.vert", options);
        auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/wireframe_pbr.frag", options);

        if (!vertexShader || !fragmentShader)
        {
            vsg::error("pbr_ShaderSet(...) could not find shaders.");
            return {};
        }

        // use the built PBR ShaderSet as a starting place
        auto shaderSet = vsg::createPhysicsBasedRenderingShaderSet(options);

        // replace the shader stages with our custom wireframe_pbr fragment shader
        shaderSet->stages = vsg::ShaderStages{vertexShader, fragmentShader};

        // clear precompiled shader variants to force use of the new shader stages.
        shaderSet->variants.clear();

        return shaderSet;
    }
}

// Ridges of up to ~70 degrees slope around a flat disc of radius 15 m at the origin.
float terrainHeight(float x, float y)
{
    const float r = std::sqrt(x * x + y * y);
    const float t = std::clamp((r - 15.0f) / 15.0f, 0.0f, 1.0f);
    const float mask = t * t * (3.0f - 2.0f * t);
    return mask * (12.0f * std::sin(x * 0.08f) * std::cos(y * 0.06f) + 6.0f * std::sin(x * 0.21f + y * 0.17f));
}

vsg::ref_ptr<vsg::Node> createTerrain(vsg::Builder& builder, float size, int cells)
{
    const int n = cells + 1;
    auto vertices = vsg::vec3Array::create(n * n);
    auto normals = vsg::vec3Array::create(n * n);
    auto texcoords = vsg::vec2Array::create(n * n);
    auto colors = vsg::vec4Array::create(1, vsg::vec4(0.8f, 0.8f, 0.8f, 1.0f));
    auto indices = vsg::uintArray::create(cells * cells * 6);

    const float step = size / static_cast<float>(cells);
    for (int j = 0; j < n; ++j)
    {
        for (int i = 0; i < n; ++i)
        {
            const float x = -0.5f * size + static_cast<float>(i) * step;
            const float y = -0.5f * size + static_cast<float>(j) * step;
            const float dx = (terrainHeight(x + 0.1f, y) - terrainHeight(x - 0.1f, y)) / 0.2f;
            const float dy = (terrainHeight(x, y + 0.1f) - terrainHeight(x, y - 0.1f)) / 0.2f;
            vertices->at(j * n + i) = vsg::vec3(x, y, terrainHeight(x, y));
            normals->at(j * n + i) = vsg::normalize(vsg::vec3(-dx, -dy, 1.0f));
            texcoords->at(j * n + i) = vsg::vec2(x / 4.0f, y / 4.0f);
        }
    }
    auto index = indices->begin();
    for (int j = 0; j < cells; ++j)
    {
        for (int i = 0; i < cells; ++i)
        {
            const uint32_t a = j * n + i;
            *index++ = a;
            *index++ = a + 1;
            *index++ = a + n;
            *index++ = a + n;
            *index++ = a + 1;
            *index++ = a + n + 1;
        }
    }

    auto draw = vsg::VertexIndexDraw::create();
    draw->assignArrays(vsg::DataList{vertices, normals, texcoords, colors});
    draw->assignIndices(indices);
    draw->indexCount = static_cast<uint32_t>(indices->size());
    draw->instanceCount = 1;

    auto stateGroup = builder.createStateGroup();
    stateGroup->addChild(draw);
    return stateGroup;
}

// Normal-offset bias: the shadow functions read eyePos; a define redirects them to
// shadowEyePos, which is moved along the normal by sin(angle to the light), scaled with
// the distance from the camera (wider texels in far cascades).
bool applyNormalOffset(vsg::ShaderSet& pbr)
{
    const std::pair<std::string, std::string> edits[] = {
        {"// include the calculateShadowCoverageForDirectionalLight(..) implementation\n",
         "vec3 shadowEyePos;\n#define eyePos shadowEyePos\n"
         "// include the calculateShadowCoverageForDirectionalLight(..) implementation\n"},
        {"// End of include code : shadows.glsl\n", "// End of include code : shadows.glsl\n#undef eyePos\n"},
        {"    int shadowMapIndex = 0;\n", "    int shadowMapIndex = 0;\n    shadowEyePos = eyePos;\n"},
        {"intensity *= (1.0-calculateShadowCoverageForDirectionalLight(lightDataIndex, shadowMapIndex, T, B, color));",
         "{\n"
         "                    float sinLight = sqrt(max(1.0 - dot(N, direction) * dot(N, direction), 0.0));\n"
         "                    shadowEyePos = eyePos + N * (sinLight * (0.02 + 0.002 * length(eyePos)));\n"
         "                    intensity *= (1.0-calculateShadowCoverageForDirectionalLight(lightDataIndex, shadowMapIndex, T, B, color));\n"
         "                    shadowEyePos = eyePos;\n"
         "                }"},
    };
    for (auto& stage : pbr.stages)
    {
        if (stage->stage != VK_SHADER_STAGE_FRAGMENT_BIT) continue;
        std::string source = stage->module->source;
        for (const auto& [from, to] : edits)
        {
            const auto at = source.find(from);
            if (at == std::string::npos) return false;
            source.replace(at, from.size(), to);
        }
        stage->module->source = source;
        stage->module->code.clear();
        pbr.variants.clear();

        std::ofstream fout("new.frag");
        fout.write(stage->module->source.data(), stage->module->source.size());
        fout.close();

        return true;
    }
    return false;
}

bool replaceShaders(vsg::ShaderSet& pbr, vsg::ref_ptr<vsg::Options> options)
{
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard.vert", options);
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("new_standard_pbr.frag", options);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("pbr_ShaderSet(...) could not find shaders.");
        return false;
    }

    vsg::info("applied new shaders");

    // replace the shader stages with our custom wireframe_pbr fragment shader
    pbr.stages = vsg::ShaderStages{vertexShader, fragmentShader};

    return true;
}

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);

    auto options = vsg::Options::create();
    options->sharedObjects = vsg::SharedObjects::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto windowTraits = vsg::WindowTraits::create(arguments);
    windowTraits->width = 1280;
    windowTraits->height = 720;
#ifdef SHADOWACNE_CAPTURE
    windowTraits->swapchainPreferences.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
#endif
    const double shadowMapBias = arguments.value<double>(0.0001, "--sb");
    const float angleSubtended = arguments.value<float>(0.0090f, "--angleSubtended");
    const double elevation = vsg::radians(arguments.value<double>(30.0, "--elevation"));
    const double distance = arguments.value<double>(45.0, "--distance");
    const uint32_t numShadowMaps = arguments.value<uint32_t>(4, "--sm");
    const bool fix = arguments.read("--fix");
    const bool new_shaders = arguments.read("--new-shaders");
    const int frames = arguments.value<int>(-1, "--frames");
    const auto screenshotFilename = arguments.value<std::string>("", "--screenshot");

    auto shaderHints = vsg::ShaderCompileSettings::create();
    vsg::ref_ptr<vsg::ShadowSettings> shadowSettings;
    float penumbraRadius = 0.05f;
    if (arguments.read("--soft", penumbraRadius))
    {
        shaderHints->defines.insert("VSG_SHADOWS_SOFT");
        shadowSettings = vsg::SoftShadows::create(numShadowMaps, penumbraRadius);
    }
    else if (arguments.read("--hard"))
    {
        shaderHints->defines.insert("VSG_SHADOWS_HARD");
        shadowSettings = vsg::HardShadows::create(numShadowMaps);
    }
    else
    {
        arguments.read("--pcss");
        shaderHints->defines.insert("VSG_SHADOWS_PCSS");
        shadowSettings = vsg::PercentageCloserSoftShadows::create(numShadowMaps);
    }
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    auto pbr = vsg::createPhysicsBasedRenderingShaderSet();
    pbr->defaultShaderHints = shaderHints;
    if (fix && !applyNormalOffset(*pbr))
    {
        std::cerr << "pbr shader source does not match the normal offset patch" << std::endl;
        return 1;
    }

    if (new_shaders && !replaceShaders(*pbr, options))
    {
        std::cerr << "new shader source not found" << std::endl;
        return 1;
    }

    auto builder = vsg::Builder::create();
    builder->shaderSet = pbr;

    auto scene = vsg::Group::create();
    scene->addChild(createTerrain(*builder, 200.0f, 256));

    vsg::GeometryInfo boxInfo;
    boxInfo.position.set(0.0f, 0.0f, 1.0f);
    boxInfo.dx.set(2.0f, 0.0f, 0.0f);
    boxInfo.dy.set(0.0f, 2.0f, 0.0f);
    boxInfo.dz.set(0.0f, 0.0f, 2.0f);
    scene->addChild(builder->createBox(boxInfo));

    auto ambient = vsg::AmbientLight::create();
    ambient->intensity = 0.05f;
    scene->addChild(ambient);

    const double azimuth = vsg::radians(30.0);
    auto sun = vsg::DirectionalLight::create();
    sun->intensity = 3.0f;
    sun->direction = -vsg::dvec3(std::cos(elevation) * std::cos(azimuth), std::cos(elevation) * std::sin(azimuth), std::sin(elevation));
    sun->angleSubtended = angleSubtended;
    sun->shadowSettings = shadowSettings;
    scene->addChild(sun);

    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window) return 1;
    viewer->addWindow(window);

    const double aspect = static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height);
    auto lookAt = vsg::LookAt::create(vsg::dvec3(-0.55, -0.65, 0.52) * distance + vsg::dvec3(10.0, 15.0, 0.0),
                                      vsg::dvec3(10.0, 15.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(vsg::Perspective::create(45.0, aspect, 0.1, 1000.0), lookAt,
                                      vsg::ViewportState::create(window->extent2D()));

    auto view = vsg::View::create(camera);
    view->viewDependentState->maxShadowDistance = 60.0;
    view->viewDependentState->shadowMapBias = shadowMapBias;
    view->addChild(scene);

    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(camera));
    viewer->assignRecordAndSubmitTaskAndPresentation({vsg::CommandGraph::create(window, vsg::RenderGraph::create(window, view))});

    auto resourceHints = vsg::ResourceHints::create();
    resourceHints->numShadowMapsRange = {numShadowMaps, numShadowMaps};
    viewer->compile(resourceHints);

    for (int frame = 0; (frames < 0 || frame < frames) && viewer->advanceToNextFrame(); ++frame)
    {
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();
    }

#ifdef SHADOWACNE_CAPTURE
    if (!screenshotFilename.empty())
    {
        viewer->deviceWaitIdle();
        return captureWindow(window, screenshotFilename) ? 0 : 1;
    }
#endif
    return 0;
}
