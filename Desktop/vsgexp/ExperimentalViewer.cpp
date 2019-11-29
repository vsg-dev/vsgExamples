#include "ExperimentalViewer.h"

using namespace vsg;

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
    if (recordAndSubmitTasks.empty())
    {
        Viewer::compile(bufferPreferences);
        return;
    }

    // TODO work out how to manage the pager
    DatabasePager* databasePager = nullptr;

    struct DeviceResources
    {
        vsg::CollectDescriptorStats collectStats;
        vsg::ref_ptr<vsg::DescriptorPool> descriptorPool;
        vsg::ref_ptr<vsg::CompileTraversal> compile;
    };

    // find which devices are available
    using DeviceResourceMap = std::map<vsg::Device*, DeviceResources>;
    DeviceResourceMap deviceResourceMap;
    for(auto& task : recordAndSubmitTasks)
    {
        for(auto& commandGraph : task->commandGraphs)
        {
            auto& deviceResources = deviceResourceMap[commandGraph->_device];
            commandGraph->accept(deviceResources.collectStats);
        }
    }

    // allocate DescriptorPool for each Device
    for(auto& [device, deviceResource] : deviceResourceMap)
    {
        auto physicalDevice = device->getPhysicalDevice();

        auto& collectStats = deviceResource.collectStats;
        auto maxSets = collectStats.computeNumDescriptorSets();
        const auto& descriptorPoolSizes = collectStats.computeDescriptorPoolSizes();

        deviceResource.compile = new vsg::CompileTraversal(device, bufferPreferences);
        deviceResource.compile->context.descriptorPool = vsg::DescriptorPool::create(device, maxSets, descriptorPoolSizes);
        deviceResource.compile->context.commandPool = vsg::CommandPool::create(device, physicalDevice->getGraphicsFamily());
        deviceResource.compile->context.graphicsQueue = device->getQueue(physicalDevice->getGraphicsFamily());

        // assign the compile traversal settings to the DatabasePager (TODO need to find a proper mechanism)
        if (databasePager) databasePager->compileTraversal = deviceResource.compile;
    }


    // create the Vulkan objects
    for(auto& task: recordAndSubmitTasks)
    {
        for(auto& commandGraph : task->commandGraphs)
        {
            auto& deviceResource = deviceResourceMap[commandGraph->_device];
            commandGraph->_maxSlot = deviceResource.collectStats.maxSlot;
            commandGraph->accept(*deviceResource.compile);
        }
    }

    // dispatch any transfer commands commands
    for(auto& [deice, deviceResource] : deviceResourceMap)
    {
        std::cout<<"Dispatching compile"<<std::endl;
        deviceResource.compile->context.dispatch();
    }

    // wait for the transfers to complete
    for(auto& [deice, deviceResource] : deviceResourceMap)
    {
        std::cout<<"Waiting for compile"<<std::endl;
        deviceResource.compile->context.waitForCompletion();
    }

    // start the pager
    if (databasePager)
    {
        databasePager->start();
    }
}

void ExperimentalViewer::recordAndSubmit()
{
    for(auto& recordAndSubmitTask : recordAndSubmitTasks)
    {
        recordAndSubmitTask->submit(_frameStamp);
    }
}

void ExperimentalViewer::present()
{
    if (presentation) presentation->present();
}
