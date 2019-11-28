#include "ExperimentalViewer.h"

using namespace vsg;

CommandGraph::CommandGraph(Device* device, int family) :
    _device(device),
    _family(family)
{
}

void CommandGraph::accept(DispatchTraversal& dispatchTraversal) const
{
    ref_ptr<CommandBuffer> commandBuffer;
    for(auto& cb : commandBuffers)
    {
        if (cb->numDependentSubmissions()==0)
        {
            commandBuffer = cb;
        }
    }
    if (!commandBuffer)
    {
        ref_ptr<CommandPool> cp = CommandPool::create(_device, _family);
        commandBuffer = CommandBuffer::create(_device, cp, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
        commandBuffers.push_back(commandBuffer);
    }

    commandBuffer->numDependentSubmissions().fetch_add(1);

    dispatchTraversal.state->_commandBuffer = commandBuffer;


    // or select index when maps to a dormant CommandBuffer
    VkCommandBuffer vk_commandBuffer = *commandBuffer;

    // need to set up the command
    // if we are nested within a CommandBuffer already then use VkCommandBufferInheritanceInfo
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    vkBeginCommandBuffer(vk_commandBuffer, &beginInfo);

    // traverse the command graph
    traverse(dispatchTraversal);

    vkEndCommandBuffer(vk_commandBuffer);
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


void RenderGraph::accept(DispatchTraversal& dispatchTraversal) const
{
    if (camera)
    {
        dmat4 projMatrix, viewMatrix;
        camera->getProjectionMatrix()->get(projMatrix);
        camera->getViewMatrix()->get(viewMatrix);

        dispatchTraversal.setProjectionAndViewMatrix(projMatrix, viewMatrix);
    }

    VkCommandBuffer vk_commandBuffer = *(dispatchTraversal.state->_commandBuffer);

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = *(window->renderPass());
    renderPassInfo.framebuffer = *(window->framebuffer(window->nextImageIndex()));
    renderPassInfo.renderArea = renderArea;

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    vkCmdBeginRenderPass(vk_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // traverse the command buffer to place the commands into the command buffer.
    traverse(dispatchTraversal);

    vkCmdEndRenderPass(vk_commandBuffer);
}


VkResult RecordAndSubmitTask::submit(ref_ptr<FrameStamp> frameStamp)
{
    std::cout<<"\n.....................................................\n";
    std::cout<<"RecordAndSubmitTask::submit()"<<std::endl;

    std::vector<VkSemaphore> vk_waitSemaphores;
    std::vector<VkPipelineStageFlags> vk_waitStages;
    std::vector<VkCommandBuffer> vk_commandBuffers;
    std::vector<VkSemaphore> vk_signalSemaphores;

    static int s_first_frame = 0;

    // aquire fence
    ref_ptr<Fence> fence;
    for (auto& window : windows)
    {
        vk_waitSemaphores.push_back(*(window->frame(window->nextImageIndex()).imageAvailableSemaphore));
        vk_waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        fence = window->frame(window->nextImageIndex()).commandsCompletedFence;
    }

    // wait on fence and clear semaphores and command buffers
    if (fence)
    {
        if ((fence->dependentSemaphores().size() + fence->dependentCommandBuffers().size()) > 0)
        {
            std::cout<<"    wait on fence = "<<fence.get()<<" "<<fence->dependentSemaphores().size()<<", "<<fence->dependentCommandBuffers().size()<<std::endl;
            uint64_t timeout = 10000000000;
            VkResult result = VK_SUCCESS;
            while ((result = fence->wait(timeout)) == VK_TIMEOUT)
            {
                std::cout << "RecordAndSubmitTask::submit(ref_ptr<FrameStamp> frameStamp)) fence->wait(" << timeout << ") failed with result = " << result << std::endl;
            }
        }
        for (auto& semaphore : fence->dependentSemaphores())
        {
            //std::cout<<"RecordAndSubmitTask::submits(..) "<<*(semaphore->data())<<" "<<semaphore->numDependentSubmissions().load()<<std::endl;
            semaphore->numDependentSubmissions().exchange(0);
        }

        for (auto& commandBuffer : fence->dependentCommandBuffers())
        {
            std::cout<<"RecordAndSubmitTask::submits(..) "<<commandBuffer.get()<<" "<<std::dec<<commandBuffer->numDependentSubmissions().load()<<std::endl;
            commandBuffer->numDependentSubmissions().exchange(0);
        }

        fence->dependentSemaphores().clear();
        fence->dependentCommandBuffers().clear();
        fence->reset();
    }

    for(auto& semaphore : waitSemaphores)
    {
        vk_waitSemaphores.emplace_back(*(semaphore));
        vk_waitStages.emplace_back(semaphore->pipelineStageFlags());
    }


    // need to compute from scene graph
    uint32_t _maxSlot = 2;

    // record the commands in the command buffer
    for(auto& commandGraph : commandGraphs)
    {
        // pass the inext to dispatchTraversal visitor?  Only for RenderGraph?
        DispatchTraversal dispatchTraversal(nullptr, _maxSlot, frameStamp);

        dispatchTraversal.databasePager = databasePager;
        if (databasePager) dispatchTraversal.culledPagedLODs = databasePager->culledPagedLODs;

        commandGraph->accept(dispatchTraversal);

        vk_commandBuffers.push_back(*(dispatchTraversal.state->_commandBuffer));

        fence->dependentCommandBuffers().emplace_back(dispatchTraversal.state->_commandBuffer);
    }

    fence->dependentSemaphores() = signalSemaphores;

    if (databasePager)
    {
        for (auto& semaphore : databasePager->getSemaphores())
        {
            if (semaphore->numDependentSubmissions().load() > 1)
            {
                std::cout << "    Warning: Viewer::submitNextFrame() waitSemaphore " << *(semaphore->data()) << " " << semaphore->numDependentSubmissions().load() << std::endl;
            }
            else
            {
                // std::cout<<"    Viewer::submitNextFrame() waitSemaphore "<<*(semaphore->data())<<" "<<semaphore->numDependentSubmissions().load()<<std::endl;
            }

            vk_waitSemaphores.emplace_back(*semaphore);
            vk_waitStages.emplace_back(semaphore->pipelineStageFlags());

            semaphore->numDependentSubmissions().fetch_add(1);
            fence->dependentSemaphores().emplace_back(semaphore);
        }
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

    std::cout<<"pdo.graphicsQueue->submit(..) fence = "<<fence.get()<<"\n";
    std::cout<<"    submitInfo.waitSemaphoreCount = "<<submitInfo.waitSemaphoreCount<<"\n";
    for(uint32_t i=0; i<submitInfo.waitSemaphoreCount; ++i)
    {
        std::cout<<"        submitInfo.pWaitSemaphores["<<i<<"] = "<<submitInfo.pWaitSemaphores[i]<<"\n";
        std::cout<<"        submitInfo.pWaitDstStageMask["<<i<<"] = "<<submitInfo.pWaitDstStageMask[i]<<"\n";
    }
    std::cout<<"    submitInfo.commandBufferCount = "<<submitInfo.commandBufferCount<<"\n";
    for(uint32_t i=0; i<submitInfo.commandBufferCount; ++i)
    {
        std::cout<<"        submitInfo.pCommandBuffers["<<i<<"] = "<<submitInfo.pCommandBuffers[i]<<"\n";
    }
    std::cout<<"    submitInfo.signalSemaphoreCount = "<<submitInfo.signalSemaphoreCount<<"\n";
    for(uint32_t i=0; i<submitInfo.signalSemaphoreCount; ++i)
    {
        std::cout<<"        submitInfo.pSignalSemaphores["<<i<<"] = "<<submitInfo.pSignalSemaphores[i]<<"\n";
    }
    std::cout<<std::endl;

    return queue->submit(submitInfo, fence);
}

VkResult Presentation::present()
{
    std::cout<<"Presentation::present()"<<std::endl;

    std::vector<VkSemaphore> vk_semaphores;
    for(auto& semaphore : waitSemaphores)
    {
        vk_semaphores.emplace_back(*(semaphore));
    }

    std::vector<VkSwapchainKHR> vk_swapchains;
    std::vector<uint32_t> indices;
    for(auto& window : windows)
    {
        //vk_semaphores.push_back(*(window->frame(window->nextImageIndex()).imageAvailableSemaphore));
        vk_swapchains.emplace_back(*(window->swapchain()));
        indices.emplace_back(window->nextImageIndex());
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(vk_semaphores.size());
    presentInfo.pWaitSemaphores = vk_semaphores.data();
    presentInfo.swapchainCount = static_cast<uint32_t>(vk_swapchains.size());
    presentInfo.pSwapchains = vk_swapchains.data();
    presentInfo.pImageIndices = indices.data();

    std::cout<<"pdo.presentInfo->present(..) \n";
    std::cout<<"    presentInfo.waitSemaphoreCount = "<<presentInfo.waitSemaphoreCount<<"\n";
    for(uint32_t i=0; i<presentInfo.waitSemaphoreCount; ++i)
    {
        std::cout<<"        presentInfo.pWaitSemaphores["<<i<<"] = "<<presentInfo.pWaitSemaphores[i]<<"\n";
    }
    std::cout<<"    presentInfo.commandBufferCount = "<<presentInfo.swapchainCount<<"\n";
    for(uint32_t i=0; i<presentInfo.swapchainCount; ++i)
    {
        std::cout<<"        presentInfo.pSwapchains["<<i<<"] = "<<presentInfo.pSwapchains[i]<<"\n";
        std::cout<<"        presentInfo.pImageIndices["<<i<<"] = "<<presentInfo.pImageIndices[i]<<"\n";
    }
    std::cout<<std::endl;

    for(auto& window : windows)
    {
        window->advanceNextImageIndex();
    }

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

    bool result = Viewer::advanceToNextFrame();

    std::cout<<"ExperimentalViewer::advanceToNextFrame() "<<result<<std::endl;

    return result;
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

    for(auto& submission : recordAndSubmitTasks)
    {
        submission->submit(_frameStamp);
    }

    if (presentation) presentation->present();

    return Viewer::submitNextFrame();
}

void ExperimentalViewer::compile(BufferPreferences bufferPreferences)
{
    // TODO, need to reorganize how Stage + command buffers are handled to collect stats or do final compile
    Viewer::compile(bufferPreferences);
}
