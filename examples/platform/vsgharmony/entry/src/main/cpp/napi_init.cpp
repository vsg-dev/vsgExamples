#include "napi/native_api.h"
//#include "HarmonyLogger.h"
#include "model_teapot.cpp"

#include <cstdint>
#include <vsg/all.h>
#include <vsg/platform/harmony/Harmony_Window.h>

// napi
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <arkui/native_interface.h>
#include <arkui/native_node.h>
#include <arkui/native_node_napi.h>
#include <hilog/log.h>  


#define LOG_INFO(...)  OH_LOG_INFO( LOG_APP, "%{public}s",  __VA_ARGS__);
#define LOG_ERROR(...)  OH_LOG_ERROR( LOG_APP, "%{public}s",   __VA_ARGS__);

ArkUI_NativeNodeAPI_1 *nodeAPI = reinterpret_cast<ArkUI_NativeNodeAPI_1 *>(
    OH_ArkUI_QueryModuleInterfaceByName(ARKUI_NATIVE_NODE, "ArkUI_NativeNodeAPI_1"));
ArkUI_AccessibilityProvider *provider_;

struct AppData
{
    ArkUI_NodeHandle handle;
    OH_ArkUI_SurfaceHolder *holder;
    OH_ArkUI_SurfaceCallback* callback;
    
    vsg::ref_ptr<vsg::WindowTraits> traits;
    vsg::ref_ptr<vsg::Viewer> viewer;
    vsg::ref_ptr<vsgHarmony::Harmony_Window> window;
    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::MatrixTransform> scene;
    
    bool initialized = false;

    bool animate = false;
    float time = 0.0f;
};
AppData *appData = nullptr;

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

void OnFrameCallback(ArkUI_NodeHandle node, uint64_t timestamp, uint64_t targetTimestamp) 
{
    OH_LOG_INFO(LOG_APP, "VSG  OnFrameCallback \n");
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
//        // update
        appData->scene->matrix = vsg::rotate(static_cast<double>(- appData->time), 0.0, 0.0, 1.0);

        // render
        appData->viewer->recordAndSubmit();
        appData->viewer->present();
    }
}

void OnSurfaceCreated(OH_ArkUI_SurfaceHolder *holder) {
    LOG_INFO("OnSurfaceCreated called\n");

    AppData *appData=static_cast<AppData*>(OH_ArkUI_SurfaceHolder_GetUserData(holder));
    if( !appData){
        LOG_ERROR("Failed to get AppData from surface holder\n");
        return;
    }

    
    OH_LOG_INFO(LOG_APP, "VSG renderer started for surface\n");
}

void OnSurfaceChanged(OH_ArkUI_SurfaceHolder *holder, uint64_t width, uint64_t height) {
    AppData *appData=static_cast<AppData*>(OH_ArkUI_SurfaceHolder_GetUserData(holder));
    if( !appData){
        LOG_ERROR("OnSurfaceChanged: Failed to get AppData from surface holder\n");
        return;
    }
    if(!appData->initialized){
        appData->initialized = true;
        
        auto nativeWindow = OH_ArkUI_XComponent_GetNativeWindow(holder); // 获取native window
        if (!nativeWindow) {
            LOG_ERROR("Failed to get native window\n");
            return;
        }
        
        // setup traits
        appData->traits = vsg::WindowTraits::create();
        appData->traits->setValue("nativeWindow", nativeWindow);
        appData->traits->width = width;
        appData->traits->height = height;
        appData->traits->debugLayer = false;
        appData->traits->apiDumpLayer = false;
    
        // create a window using the ANativeWindow passed via traits
        vsg::ref_ptr<vsg::Window> vsgWindow;
        try {
            vsgWindow = vsg::Window::create(appData->traits);
        } catch( const std::bad_any_cast& e ) {
            LOG_ERROR("Error: Failed to create a VSG Window due to std::bad_any_cast - The application was linked to the C++ STL incorrectly");
            throw;
        }
        if (!vsgWindow)
        {
            LOG_INFO("Error: Could not create a VSG window.");
            return ;
        }
        // cast the window to an android window so we can pass it events
        appData->window = vsgWindow.cast<vsgHarmony::Harmony_Window>();
        
        appData->viewer = vsg::Viewer::create();
        // attach the window to the viewer
        appData->viewer->addWindow(vsgWindow);
        
        appData->scene = vsg::MatrixTransform::create();
        auto rot = vsg::MatrixTransform::create(vsg::rotate(vsg::radians(90.0), 1.0, 0.0, 0.0));
        appData->scene->addChild(rot);
        rot->addChild( teapot() );
        
        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        vsg::ref_ptr<vsg::LookAt> lookAt;
        calculateCameraParameters(appData->scene, appData->window, lookAt, perspective);
        appData->camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(appData->window->extent2D()));
    
        auto commandGraph = vsg::createCommandGraphForView(appData->window, appData->camera, appData->scene);
        appData->viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
        appData->viewer->addEventHandler(vsg::Trackball::create(appData->camera));
        appData->viewer->compile();
        
        OH_ArkUI_XComponent_RegisterOnFrameCallback(appData->handle, OnFrameCallback);
    }
    
}

void OnSurfaceDestroyed(OH_ArkUI_SurfaceHolder *holder) 
{
    OH_ArkUI_SurfaceHolder_Dispose(holder); 
    OH_LOG_INFO(LOG_APP, "VSG  OnSurfaceDestroyed \n");
 
}

void OnSurfaceShow(OH_ArkUI_SurfaceHolder *holder) 
{
     OH_LOG_INFO(LOG_APP, "VSG  OnSurfaceShow \n");
}

void OnSurfaceHide(OH_ArkUI_SurfaceHolder *holder) 
{
     OH_LOG_INFO(LOG_APP, "VSG  OnSurfaceHide \n");
}

void onEvent(ArkUI_NodeEvent *event) {
    auto eventType = OH_ArkUI_NodeEvent_GetEventType(event);
    if (eventType == NODE_TOUCH_EVENT) {
        OH_LOG_INFO(LOG_APP, "VSG  on event\n");
        //ArkUI_NodeHandle handle = OH_ArkUI_NodeEvent_GetNodeHandle(event);
        AppData *appData=static_cast<AppData*>(OH_ArkUI_NodeEvent_GetUserData(event));
        if(appData)
            appData->window->handleOHOSInputEvent(event);
    }
}

napi_value BindNode(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    appData = new AppData();

    OH_ArkUI_GetNodeHandleFromNapiValue(env, args[0], &appData->handle);      
    appData->holder = OH_ArkUI_SurfaceHolder_Create(appData->handle); 

    appData->callback = OH_ArkUI_SurfaceCallback_Create(); // 创建SurfaceCallback

    OH_ArkUI_SurfaceCallback_SetSurfaceCreatedEvent(appData->callback, OnSurfaceCreated);     
    OH_ArkUI_SurfaceCallback_SetSurfaceChangedEvent(appData->callback, OnSurfaceChanged);     
    OH_ArkUI_SurfaceCallback_SetSurfaceDestroyedEvent(appData->callback, OnSurfaceDestroyed); 
    OH_ArkUI_SurfaceCallback_SetSurfaceShowEvent(appData->callback, OnSurfaceShow);           
    OH_ArkUI_SurfaceCallback_SetSurfaceHideEvent(appData->callback, OnSurfaceHide);           
        
    OH_ArkUI_SurfaceHolder_AddSurfaceCallback(appData->holder, appData->callback); 
    if (!nodeAPI->addNodeEventReceiver(appData->handle, onEvent)) {      
        // OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "onBind", "addNodeEventReceiver error");
    }
    if (!nodeAPI->registerNodeEvent(appData->handle, NODE_TOUCH_EVENT, 0, appData)) {
        // OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "onBind", "registerTouchEvent error");
    }
    OH_ArkUI_SurfaceHolder_SetUserData(appData->holder, appData);
    
    return nullptr;
}

napi_value UnbindNode(napi_env env, napi_callback_info info) {
//    size_t argc = 1;
//    napi_value args[1] = {nullptr};
//    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
//    int64_t appDataPtr;
//    napi_get_value_int64(env,  args[0], &appDataPtr);
//    AppData *appData = (AppData *)appDataPtr;
    OH_ArkUI_XComponent_UnregisterOnFrameCallback(appData->handle);
    nodeAPI->disposeNode(appData->handle);
    delete appData;
    return nullptr;
}


EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        {"bindNode", nullptr, BindNode, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"unbindNode", nullptr, UnbindNode, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&demoModule);
}
