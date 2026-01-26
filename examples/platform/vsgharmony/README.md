# VSG Example for HarmonyOS

This example demonstrates how to create a window, load assets and render a scene within a XComponent application.

Note: this example focuses on VSG usage,not the best development practice for HarmonyOS NativeWindow

## Prerequisites

    * DevEco Studio 6+
    * Harmony SDK API Level 19
    * CMake 3.5
    * Device running HarmonyOS 6 or higher which supports Vulkan 1.0 (The DevEco's semulator does not support valkan) 


## Building with DevEco Studio

    1. Open DevEco Studio and select 'Open'
    2. Select the vsgharmony folder
    3. Open /src/main/cpp/CMakeLists.txt and update the section 'find vsg' to either:
       * `set(CMAKE_PREFIX_PATH  /path/to/VulkanSceneGraph vsg )`
    4 Modify the line `set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)` in the file `ohos.toolchain.cmake` to prevent `find_package` from failing to find libraries outside the cross-compilation environment.
    5. Build the application

