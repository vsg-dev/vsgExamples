# vsgExamples
Example programs that test and illustrate how to use the [VulkanSceneGraph/VkSceneGraph](https://github.com/vsg-dev/VulkanSceneGraphPrototype/) and optional add-on libraries. The example programs are grouped according to their dependencies:

## examples_vsg
Contains [examples](examples_vsg/) that depend upon the core **vsg** library provided by the [VulkanSceneGraphPrototype](https://github.com/vsg-dev/VulkanSceneGraphPrototype/) and [Vulkan](https://vulkan.lunarg.com/).

## examples_osg2vsg
Contains [examples](examples_osg2vsg/) that depend upon the core **vsg** library, **Vulkan**, and add the dependency on [osg2vsg](https://github.com/vsg-dev/osg2vsg/) library which in turn depends upon [OpenSceneGraph](https://github.com/openscenegraph/OpenSceneGraph/). The examples_osg2vsg are only built when osg2vsg is available.

## Quick Guide to Building the vsgExamples

### Prerequisites:
* C++17 compliant compiler i.e.. g++ 7.3 or later, Clang 6.0 or later, Visual Studio S2017 or later.
* [Vulkan](https://vulkan.lunarg.com/) 1.1 or later.
* [CMake](https://www.cmake.org) 3.7 or later.
* [GLFW](https://www.glfw.org)  3.3 or later.  Used internally by libvsg. The plan is to implement native Windowing support so this dependency will later be removed.
* [VulkanSceneGraphPrototype](https://github.com/vsg-dev/VulkanSceneGraphPrototype/) use master

### Optional Dependencies:
* [osg2vsg](https://github.com/vsg-dev/osg2vsg/) use master.
* [OpenSceneGraph](https://github.com/openscenegraph/OpenSceneGraph/), currently any 3.x stable release should work fine.

The above dependency versions are known to work so they've been set as the current minimum, it may be possible to build against older versions.  If you find success with older versions let us know and we can update the version info.

### Command line build instructions:
To build and install in source, with all dependencies installed in standard system directories:

    git clone https://github.com/vsg-dev/vsgExamples.git
    cd vsgExmples
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
