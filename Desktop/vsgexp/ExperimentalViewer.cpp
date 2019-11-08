#include "ExperimentalViewer.h"

using namespace vsg;

// TODO
// Replace Stage
// Replace
//
// Viewer
//        struct PerDeviceObjects
//        {
//            Windows windows;
//            ref_ptr<Queue> graphicsQueue;
//            ref_ptr<Queue> presentQueue;
//            ref_ptr<Semaphore> renderFinishedSemaphore;
//
//            // cache data to be used each frame
//            std::vector<uint32_t> imageIndices;
//            std::vector<VkSemaphore> signalSemaphores;
//            std::vector<VkCommandBuffer> commandBuffers;
//            std::vector<VkSwapchainKHR> swapchains;
//            std::vector<VkPipelineStageFlags> waitStages;
//        };

ExperimentalViewer::ExperimentalViewer()
{
}

ExperimentalViewer::~ExperimentalViewer()
{
    // TODO, need handle clean up of threads and Vulkan queues
}

void ExperimentalViewer::addWindow(ref_ptr<Window> window)
{
    // TODO, need to review PerDeviceObjects setup
    Viewer::addWindow(window);
}

bool ExperimentalViewer::advanceToNextFrame()
{
    // Probably OK
    return Viewer::advanceToNextFrame();
}

void ExperimentalViewer::advance()
{
    // Probably OK
    Viewer::advance();
}

void ExperimentalViewer::handleEvents()
{
    // Probably OK
    Viewer::handleEvents();
}

void ExperimentalViewer::reassignFrameCache()
{
    // TODO, need to review PerDeviceObjects
    Viewer::reassignFrameCache();
}

bool ExperimentalViewer::acquireNextFrame()
{
    // Probably OK
    return Viewer::acquireNextFrame();
}

bool ExperimentalViewer::populateNextFrame()
{
    // TODO, need to replace the Window::populate.. calls
    return Viewer::populateNextFrame();
}

bool ExperimentalViewer::submitNextFrame()
{
    // TODO, need to replace the PerDeviceObjects / Window / Fence and Semaphore handles
    return Viewer::submitNextFrame();
}

void ExperimentalViewer::compile(BufferPreferences bufferPreferences)
{
    // TODO, need to reorganize how Stage + command buffers are handled to collect stats or do final compile
    Viewer::compile(bufferPreferences);
}
