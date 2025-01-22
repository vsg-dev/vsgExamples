#define NOMINMAX
#include <windows.h>

#include <thread>
#include <vsg/all.h>
#include <vsg/platform/win32/Win32_Window.h>

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
char szClassName[] = "VSG";

void startViewer(vsg::Viewer* viewer)
{
    viewer->compile();
    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();
    }
}

/**
 * @brief This small example is intended to show how VSG can be integrated into an existing Win32
 * application. It also shows how the VSG event handler can be reused for basic functions such
 * as rotating and zooming.
 *
 */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR lpszCmdLine, int nCmdShow)
{
    HWND hWnd;
    MSG msg;
    WNDCLASS myProg;

    if (!hPreInst)
    {
        myProg.style = CS_HREDRAW | CS_VREDRAW;
        myProg.lpfnWndProc = WndProc; //register event handler
        myProg.cbClsExtra = 0;
        myProg.cbWndExtra = 0;
        myProg.hInstance = hInstance;
        myProg.hIcon = NULL;
        myProg.hCursor = LoadCursor(NULL, IDC_ARROW);
        myProg.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
        myProg.lpszMenuName = NULL;
        myProg.lpszClassName = szClassName;
        if (!RegisterClass(&myProg))
            return FALSE;
    }
    hWnd = CreateWindow(szClassName, "VSG Win32 Window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // setup VSG
    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->nativeWindow = hWnd;

    auto window = vsg::Window::create(windowTraits);
    // register the vsgWin32::Win32_Window object so that we can have events processed by the VSG event handler
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window.get()));

    window->clearColor() = vsg::sRGB_to_linear(0.2f, 0.2f, 0.2f, 0.0f);
    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);

    auto lookAt = vsg::LookAt::create(vsg::dvec3(0.0, -10.0, 0.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));

    double nearFarRatio = 0.001;
    double radius = 1.0;

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 100);

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));
    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // add trackball to control the Camera
    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto group = vsg::Group::create();
    vsg::GeometryInfo geomInfo;
    geomInfo.dx.set(1.0f, 0.0f, 0.0f);
    geomInfo.dy.set(0.0f, 1.0f, 0.0f);
    geomInfo.dz.set(0.0f, 0.0f, 1.0f);

    vsg::Builder builder;
    vsg::ref_ptr<vsg::Node> node = builder.createCylinder(geomInfo);
    group->addChild(node);

    // add the CommandGraph to render the scene
    auto commandGraph = vsg::createCommandGraphForView(window, camera, group);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    std::thread* thread = new std::thread(startViewer, (vsg::Viewer*)viewer.get());

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    viewer->close();
    thread->join();
    delete thread;

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // let VSG's event handler process events
    vsgWin32::Win32_Window* win = reinterpret_cast<vsgWin32::Win32_Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (win != nullptr)
    {
        win->handleWin32Messages(msg, wParam, lParam);
    }

    // process/react to events yourself if required
    switch (msg)
    {
    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    case WM_MOUSEMOVE: {
        break;
    }
    case WM_KEYDOWN: {
        switch (wParam)
        {
        case VK_LEFT: {
            break;
        }
        case VK_UP: {
            break;
        }
        case VK_RIGHT: {
            break;
        }
        }
    }
    // ...
    default:
        return ::DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
