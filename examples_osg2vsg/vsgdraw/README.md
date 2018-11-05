## vsgdraw - port of VulkanTutorial
The vsgdraw example was developed during the ExplorationPhase as a test bed of the VSG Vulkan integration. 

The [VulkanTutorial](https://vulkan-tutorial.com/) was used as a guide for what Vulkan calls are required to create a window, set up state and geometry data for two textured quads, and render to depth buffered window.  Feature wise the vsgdraw matches up to Depth Buffer stage of the tutorial, but is very different in C++ style and usage virtual of use of the VSG's Vulkan integration streamlining the Vulkan setup.

The vsgdraw, at 258 lines of code, is far more streamlined and readable than the original VulkanTutorial's [26_depth_buffering.cpp](https://github.com/Overv/VulkanTutorial/blob/master/code/26_depth_buffering.cpp) which has 1530 lines of code.

To run vsgdraw you will need to set the path to the shaders and textures by setting the env var **VSG_FILE_PATH** to the [vsgExamples/data](../../data) directory.
