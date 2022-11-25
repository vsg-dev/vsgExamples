# VSG Native Activity Example for Android

This example demonstates how to create a window, load assests and render a scene within a Native Activity application. It uses Gradle and CMake and is compatible with Android Studio. 

Note: that this example focuses on VSG usage, and doesn't use the latest Android API versions or similar

## Prerequisites

    * Android Studio
    * Android SDK API Level 24
    * Android NDK 25
    * CMake 3.13
    * Device running Android 7 or higher which supports Vulkan 1.0


## Building with Android Studio

    1. Open Android Studio and select 'Import Project'
    2. Select the vsgandroidnative folder
    3. Open /app/cpp/CMakeLists.txt and update the section 'find vsg' to either:
       * `add_subdirectory( /path/to/VulkanSceneGraph vsg )`
       * `find_package( vsg REQUIRED NO_CMAKE_FIND_ROOT_PATH )` and update build.gradle to pass `-Dvsg_DIR=/path/to/vsg/android/installation` 

    4. Build the application

