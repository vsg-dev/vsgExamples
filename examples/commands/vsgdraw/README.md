## vsgdraw - port of VulkanTutorial
The *vsgdraw* example was developed during the ExplorationPhase as a test bed of the VSG Vulkan integration.

The [VulkanTutorial](https://vulkan-tutorial.com/) was used as a guide for what Vulkan calls are required to create a window, set up state and geometry data for two textured quads, and render to depth buffered window.  Feature wise the vsgdraw matches up to Depth Buffer stage of the tutorial, but is very different in C++ style and usage virtual of use of the VSG's Vulkan integration streamlining the Vulkan setup.

The [vsgdraw.cpp](vsgdraw.cpp), at 192 lines of code, is far more streamlined and readable than the original VulkanTutorial's [26_depth_buffering.cpp](https://github.com/Overv/VulkanTutorial/blob/master/code/26_depth_buffering.cpp) which has 1530 lines of code.

To run *vsgdraw* you will need to set the path to the shaders and textures by setting the env var **VSG_FILE_PATH** to the [vsgExamples/data](../../data) directory, and add the vsgExamples/bin directory to your system binary path. i.e

Linux:

	export VSG_FILE_PATH=/path/to/vsgExamples/data
	export PATH=/path/to/vsgExamples/bin:${PATH}

Windows:

	set VSG_FILE_PATH drive:\path\to\vsgExamples\data
	set PATH drive:\path\to\vsgExamples\bin

*vsgdraw* can be run without command line options, and will create a window with two textured quads rotating on screen.  Press Escape to close the window.

	vsgdraw

*vsgdraw* also supports a set of command line options:

	-d or --debug        # enable the Vulkan debug layer, outputs to console
	-a or --api          # enable Vulkan API dump layer, outputs all vulkan calls
	-f                   # specify the number of frames to render before exit
	-w or --window <width> <height> # specify the dimension of the window to open
