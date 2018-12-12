## Building the vsgExamples

### Prerequisites:
* C++17 compliant compiler i.e.. g++ 7.3 or later, Clang 6.0 or later, Visual Studio S2017 or later.
* [Vulkan](https://vulkan.lunarg.com/) 1.1 or later.
* [CMake](https://www.cmake.org) 3.7 or later.
* [VulkanSceneGraphPrototype](https://github.com/vsg-dev/VulkanSceneGraphPrototype/) use master
* Under macOS : [GLFW](https://www.glfw.org)  3.3 or later.  The plan is to implement native Windowing support so this dependency will later be removed. The VSG has native Windows, Linux and Android windowing support so do not require GLFW.

### Optional Dependencies:
* [OpenSceneGraph](https://github.com/openscenegraph/OpenSceneGraph/), currently any 3.x stable release should work fine.

The above dependency versions are known to work so they've been set as the current minimum, it may be possible to build against older versions.  If you find success with older versions let us know and we can update the version info.

### Command line build instructions:
To build in source, with all dependencies installed in standard system directories:

    git clone https://github.com/vsg-dev/vsgExamples.git
    cd vsgExmples
    cmake .
    make -j 8

To build out of source, with all dependencies installed in standard system directories:

    git clone https://github.com/vsg-dev/vsgExamples.git
    mkdir build-vsgExamples
    cd build-vsgExamples
    cmake .
    make -j 8
