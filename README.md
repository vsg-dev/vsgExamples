# vsgExamples
Example programs that test and illustrate how to use the [VulkanSceneGraph/VkSceneGraph](https://github.com/vsg-dev/VulkanSceneGraphPrototype/) and optional add-on libraries. The example programs are grouped according to the type of features they illustrate their dependencies.

## Core examples
The [Core](Core/) directory contains examples illustrate core class features of the VSG such as reference counting, maths, visitors etc.

## Desktop examples
The [Desktop](Desktop/) directory contains examples use the VSG's native Windowing to create various different desktop graphics applications

## Android examples
The [Android](Android/) directory contains examples illustrate how to create Android graphics applications

## Quick Guide to Building the vsgExamples

### Prerequisites:
* C++17 compliant compiler i.e.. g++ 7.3 or later, Clang 6.0 or later, Visual Studio 2017 or later.
* [Vulkan](https://vulkan.lunarg.com/) 1.1 or later.
* [CMake](https://www.cmake.org) 3.7 or later.
* [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph/) use master

The above dependency versions are known to work so they've been set as the current minimum, it may be possible to build against older versions.  If you find success with older versions let us know and we can update the version info.

### Command line build instructions:
To build and install in source, with all dependencies installed in standard system directories:

    git clone https://github.com/vsg-dev/vsgExamples.git
    cd vsgExamples
    cmake .
    make -j 8

Full details on how to build of the VSG can be found in the [INSTALL.md](INSTALL.md) file.

## Running examples

After you have built the examples you should set your binary search path to the vsgExamples/bin directory, and the VSG_FILE_PATH env vars.

	export PATH=/path/to/vsgExamples/bin
	export VSG_FILE_PATH=/path/to/vsgExamples/data

Then run examples:

	vsgmaths # run simple tests of vsg/maths functionality
	vsgdraw # run the vsgdraw example (a port of VulkanTutorial)
