# VSG Native Activity Example for Android

This example demonstates how to create a window, load assests and render a scene within a Native Activity application. It uses Gradle and CMake and is compatible with Android Studio. 


## Prerequisites

    * Android Studio
    * Android SDK API Level 28
    * Android NDK 18
    * CMake 3.13
    * Device running Android 7 or higher which supports Vulkan 1.0


## Building with Android Studio

    1. Open Android Studio and select 'Import Project'
    2. Select the vsgandroidnative folder
    3. Once the project loads open the generated 'local.properties' file
    4. At the bottom of the file add the path to your cmake directory (v3.13) like so
       cmake.dir=/usr/local/Cellar/cmake/3.13.0
    5. Open /app/build.gradle and change the '-Dvsg_DIR' path in the cmake arguments section to point to your Android VSG install path.
	  * Alternatively, edit app/cpp/CMakeLists.txt and add replace find_package(vsg) with add_subdirectory(/path/to/VulkanSceneGraph vsg). This will build vsg from source, as part of the Android app's CMake project
	  * Note that when using -Dvsg_DIR or CMAKE_PREFIX_PATH on Android, only paths within the Android sysroot are allowed by default. You can use find_package(vsg NO_CMAKE_FIND_ROOT_PATH) to override this behaviour
    6. Build the application
    
    
## Running on device

Before you can run the VSG Native example on your device you need to first copy the texture and shader files on to you device. Take the 'data' folder located in the root of the vsgExamples repo and copy it to the 'Download' folder of your Android device.

At this point  hit 'Run', select your device and install VSG Native on to your device. 

Important! The first time you do this the example is going to crash. This is because you need to accept the 'Storage' permmision for the application to read the shaders and texture from the Download folder. To do this go to 'Settings>Apps>VSG Native>Permisions' and enable 'Storage'. Now if you lanuch VSG Native again it will run.
     
