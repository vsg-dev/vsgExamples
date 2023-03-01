

#include <vsg/viewer/Window.h>
#include <vsg/viewer/Viewer.h>
#include <vsg/platform/ios/iOS_Window.h>
#import <UIKit/UIKit.h>

#include <vsg/all.h>

#include "lz.cpp"
using namespace vsg;

//------------------------------------------------------------------------
// Application delegate
//------------------------------------------------------------------------
@interface vsgiOSAppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) vsg_iOS_Window *window;
@property CADisplayLink* displayLink;
@property vsg::ref_ptr<vsg::Viewer> vsgViewer;
@end



@implementation vsgiOSAppDelegate


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    
    auto mainScreen =[UIScreen mainScreen];
    auto bounds = [mainScreen bounds];
    
    vsg::ref_ptr<vsg::Viewer> vsgViewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::WindowTraits> traits = vsg::WindowTraits::create();
    traits->x = bounds.origin.x;
    traits->y = bounds.origin.y;
    traits->width = bounds.size.width;
    traits->height = bounds.size.height;
    self.window = [[vsg_iOS_Window alloc] initWithTraits: traits andVsgViewer: vsgViewer];
    self.window.makeKeyAndVisible;


    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto loadLevels=0;
    auto horizonMountainHeight=0;
    std::string pathFilename = "";
    vsg::ref_ptr<vsg::Window> window = self.window.vsgWindow;
    vsg::ref_ptr<vsg::Node> vsg_scene = lz();

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
    if (ellipsoidModel)
    {
       perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
    }
    else
    {
       perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond the close window button and pressing escape
    vsgViewer->addEventHandler(vsg::CloseHandler::create(vsgViewer));

    if (pathFilename.empty())
    {
       vsgViewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));
    }
    else
    {
       auto animationPath = vsg::read_cast<vsg::AnimationPath>(pathFilename, options);
       if (!animationPath)
       {
           std::cout<<"Warning: unable to read animation path : "<<pathFilename<<std::endl;
           return 1;
       }

       auto animationPathHandler = vsg::AnimationPathHandler::create(camera, animationPath, vsgViewer->start_point());
       animationPathHandler->printFrameStatsToConsole = true;
       vsgViewer->addEventHandler(animationPathHandler);
    }

    // if required preload specific number of PagedLOD levels.
    if (loadLevels > 0)
    {
       vsg::LoadPagedLOD loadPagedLOD(camera, loadLevels);

       auto startTime = std::chrono::steady_clock::now();

       vsg_scene->accept(loadPagedLOD);

        auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::steady_clock::now() - startTime).count();
        std::cout << "No. of tiles loaded " << loadPagedLOD.numTiles << " in " << time << "ms." << std::endl;
    }

    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
    vsgViewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    vsgViewer->compile();
    self.vsgViewer = vsgViewer;
  
    // now setup the render loop
    _displayLink = [CADisplayLink displayLinkWithTarget: self selector: @selector(renderLoop)];
    uint32_t fps = 60;
    [_displayLink setPreferredFramesPerSecond: fps];
    [_displayLink addToRunLoop: NSRunLoop.currentRunLoop forMode: NSDefaultRunLoopMode];

    return YES;
}

-(void) renderLoop {
   if (self.vsgViewer->advanceToNextFrame())
   {
       self.vsgViewer->compile();
       self.vsgViewer->handleEvents();
       self.vsgViewer->update();
       self.vsgViewer->recordAndSubmit();
       self.vsgViewer->present();
   }
}


- (void)applicationWillResignActive:(UIApplication *)application {
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
    // Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}

- (void)applicationWillTerminate:(UIApplication *)application {
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}


@end

int main(int argc, char * argv[]) {
    @autoreleasepool {
          return UIApplicationMain(argc, argv, nil, @"vsgiOSAppDelegate");
    }
}
	
