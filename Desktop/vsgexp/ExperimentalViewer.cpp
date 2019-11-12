#include "ExperimentalViewer.h"

using namespace vsg;

CommandGraph::CommandGraph()
{
#if 0
    ref_ptr<CommandPool> cp = CommandPool::create(_device, _physicalDevice->getGraphicsFamily());
    commandBuffer = CommandBuffer::create(_device, cp, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

    // set up the dispatching of the commands into the command buffer
    dispatchTraversal = DispatchTraversal::create(commandBuffer, _maxSlot, frameStamp);
    dispatchTraversal->setProjectionAndViewMatrix(_projMatrix->value(), _viewMatrix->value());

    dispatchTraversal->databasePager = databasePager;
    if (databasePager) dispatchTraversal.culledPagedLODs = databasePager->culledPagedLODs;
#endif
}

RenderGraph::RenderGraph()
{
#if 0
    renderPass; // provided by window
    framebuffer // provided by window/swapchain
    renderArea; // provide by camera?
    clearValues; // provide by camera?
#endif
}


VkResult Submission::submit(ref_ptr<FrameStamp> frameStamp)
{
    std::vector<VkSemaphore> vk_waitSemaphores;
    std::vector<VkPipelineStageFlags> vk_waitStages;
    std::vector<VkCommandBuffer> vk_commandBuffers;
    std::vector<VkSemaphore> vk_signalSemaphores;


    for(auto& semaphore : waitSemaphores)
    {
        vk_waitSemaphores.emplace_back(*(semaphore));
        vk_waitStages.emplace_back(semaphore->pipelineStageFlags());
    }

    for(auto& commandGraph : commandGraphs)
    {
        // pull the command buffer
        VkCommandBuffer vk_commandBuffer = *(commandGraph->commandBuffer);

        // need to set up the command
        // if we are nested within a CommandBuffer already then use VkCommandBufferInheritanceInfo
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        vkBeginCommandBuffer(vk_commandBuffer, &beginInfo);

        // traverse the command graph
        commandGraph->traverse(*(commandGraph->dispatchTraversal));

        vkEndCommandBuffer(vk_commandBuffer);

        vk_commandBuffers.emplace_back(vk_commandBuffer);
    }

    for(auto& semaphore : signalSemaphores)
    {
        vk_signalSemaphores.emplace_back(*(semaphore));
    }


    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(vk_waitSemaphores.size());
    submitInfo.pWaitSemaphores = vk_waitSemaphores.data();
    submitInfo.pWaitDstStageMask = vk_waitStages.data();

    submitInfo.commandBufferCount = static_cast<uint32_t>(vk_commandBuffers.size());
    submitInfo.pCommandBuffers = vk_commandBuffers.data();

    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(vk_signalSemaphores.size());
    submitInfo.pSignalSemaphores = vk_signalSemaphores.data();

    return queue->submit(submitInfo, fence);
}

VkResult Presentation::present()
{
    std::vector<VkSemaphore> vk_semaphores;
    for(auto& semaphore : waitSemaphores)
    {
        vk_semaphores.emplace_back(*(semaphore));
    }

    std::vector<VkSwapchainKHR> vk_swapchains;
    for(auto& swapchain : swapchains)
    {
        vk_swapchains.emplace_back(*(swapchain));
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(vk_semaphores.size());
    presentInfo.pWaitSemaphores = vk_semaphores.data();
    presentInfo.swapchainCount = static_cast<uint32_t>(vk_swapchains.size());
    presentInfo.pSwapchains = vk_swapchains.data();
    presentInfo.pImageIndices = indices.data();

    return queue->present(presentInfo);
}

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
    //       could just do nothing here, rely upon Submission to do the required update.
    return Viewer::populateNextFrame();
}

bool ExperimentalViewer::submitNextFrame()
{
    // TODO, need to replace the PerDeviceObjects / Window / Fence and Semaphore handles

    for(auto& submission : submissions)
    {
        submission.submit(_frameStamp);
    }

    if (presentation) presentation->present();

    return Viewer::submitNextFrame();
}

void ExperimentalViewer::compile(BufferPreferences bufferPreferences)
{
    // TODO, need to reorganize how Stage + command buffers are handled to collect stats or do final compile
    Viewer::compile(bufferPreferences);
}
