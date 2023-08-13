
#include <initializer_list>
//#include <memory>
#include <cstdlib>
#include <cstring>
#include <jni.h>

#include <android/log.h>
#include <android_native_app_glue.h>

#include <vsg/all.h>
#include <vsg/platform/android/Android_Window.h>

#include "model_teapot.cpp"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "vsgnative", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "vsgnative", __VA_ARGS__))

//
// App state
//
struct AppData
{
    struct android_app* app = nullptr;

    vsg::ref_ptr<vsg::WindowTraits> traits;
    vsg::ref_ptr<vsg::Viewer> viewer;
    vsg::ref_ptr<vsgAndroid::Android_Window> window;
    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::MatrixTransform> scene;

    bool animate = false;
    float time = 0.0f;
};

static void calculateCameraParameters(vsg::ref_ptr<vsg::Node> scene, vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::LookAt>& lookAt, vsg::ref_ptr<vsg::ProjectionMatrix>& projectionMatrix)
{
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    lookAt = vsg::LookAt::create(centre + vsg::dvec3(-radius * 3.5, 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    projectionMatrix = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 6);
}

//
// Init the vsg viewer and load assets
//
static int vsg_init(struct AppData* appData)
{
    appData->animate = true;
    appData->time = 0.0f;

    appData->viewer = vsg::Viewer::create();

    // setup traits
    appData->traits = vsg::WindowTraits::create();
    appData->traits->setValue("nativeWindow", appData->app->window);
    appData->traits->width = ANativeWindow_getWidth(appData->app->window);
    appData->traits->height = ANativeWindow_getHeight(appData->app->window);

    // create a window using the ANativeWindow passed via traits
    vsg::ref_ptr<vsg::Window> window;
    try {
        window = vsg::Window::create(appData->traits);
    } catch( const std::bad_any_cast& e ) {
        LOGW("Error: Failed to create a VSG Window due to std::bad_any_cast - The application was linked to the C++ STL incorrectly");
        throw;
    }
    if (!window)
    {
        LOGW("Error: Could not create a VSG window.");
        return 1;
    }

    appData->scene = vsg::MatrixTransform::create();
    auto rot = vsg::MatrixTransform::create(vsg::rotate(vsg::radians(90.0), 1.0, 0.0, 0.0));
    appData->scene->addChild(rot);
    rot->addChild( teapot() );

    // cast the window to an android window so we can pass it events
    appData->window = window.cast<vsgAndroid::Android_Window>();

    // attach the window to the viewer
    appData->viewer->addWindow(window);

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    vsg::ref_ptr<vsg::LookAt> lookAt;
    calculateCameraParameters(appData->scene, window, lookAt, perspective);
    appData->camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    auto commandGraph = vsg::createCommandGraphForView(window, appData->camera, appData->scene);
    appData->viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    appData->viewer->compile();

    return 0;
}

static void vsg_term(struct AppData* appData)
{
    appData->traits = nullptr;
    appData->viewer = nullptr;
    appData->window = nullptr;
    appData->camera = nullptr;
    appData->animate = false;
}

//
// Render a frame
//
static void vsg_frame(struct AppData* appData)
{
    if (!appData->viewer.valid()) {
        // no viewer.
        return;
    }

    if(appData->viewer->advanceToNextFrame()) {
        // poll events and advance frame counters
        // pass any events into EventHandlers assigned to the Viewer
        appData->viewer->handleEvents();

        appData->viewer->update();
        appData->time = appData->time + 0.033f;

        // update
        appData->scene->matrix = vsg::rotate(static_cast<double>(- appData->time), 0.0, 0.0, 1.0);

        // render
        appData->viewer->recordAndSubmit();
        appData->viewer->present();
    }

}

//
// Handle input event
//
static int32_t android_handleinput(struct android_app* app, AInputEvent* event)
{
    struct AppData* appData = (struct AppData*)app->userData;
    if(appData->window.valid()) return 0;
    // pass the event to the vsg android window
    return appData->window->handleAndroidInputEvent(event) ? 1 : 0;
}

//
// Handle main commands
//
static void android_handlecmd(struct android_app* app, int32_t cmd)
{
    struct AppData* appData = (struct AppData*)app->userData;
    if(appData == nullptr) return;

    switch (cmd)
    {
        case APP_CMD_SAVE_STATE:
            break;
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            if (app->window != nullptr)
            {
                vsg_init(appData);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            vsg_term(appData);
            break;
        case APP_CMD_GAINED_FOCUS:
            appData->animate = true;
            break;
        case APP_CMD_LOST_FOCUS:
            appData->animate = false;
            break;
    }
}

//
// Main entry point for the application
//
void android_main(struct android_app* app)
{
    struct AppData appData;

    app->userData = &appData;
    app->onAppCmd = android_handlecmd;
    app->onInputEvent = android_handleinput;
    appData.app = app;

    // Used to poll the events in the main loop
    int events;
    android_poll_source* source;

    // main loop
    do
    {
        // poll events
        if (ALooper_pollAll(0, nullptr, &events, (void**)&source) >= 0)
        {
            if (source != nullptr) source->process(app, source);
        }

        // render if vulkan is ready and we are animating
        if (appData.animate && appData.viewer.valid()) {
            vsg_frame(&appData);
        }
    } while (app->destroyRequested == 0);
}
