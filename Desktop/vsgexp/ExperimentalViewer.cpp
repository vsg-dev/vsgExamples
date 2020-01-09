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


//
// new
//

void ExperimentalViewer::compile(BufferPreferences bufferPreferences)
{
    if (recordAndSubmitTasks.empty())
    {
        Viewer::compile(bufferPreferences);
        return;
    }

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
    }


    // create the Vulkan objects
    for(auto& task: recordAndSubmitTasks)
    {
        std::set<Device*> devices;

        for(auto& commandGraph : task->commandGraphs)
        {
            if (commandGraph->_device) devices.insert(commandGraph->_device);

            auto& deviceResource = deviceResourceMap[commandGraph->_device];
            commandGraph->_maxSlot = deviceResource.collectStats.maxSlot;
            commandGraph->accept(*deviceResource.compile);
        }

        if (task->databasePager)
        {
            // crude hack for taking first device as the one for the DatabasePager to compile resourcces for.
            for(auto& commandGraph : task->commandGraphs)
            {
                auto& deviceResource = deviceResourceMap[commandGraph->_device];
                task->databasePager->compileTraversal = deviceResource.compile;
                break;
            }
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

    // start any DatabasePagers
    for(auto& task: recordAndSubmitTasks)
    {
        if (task->databasePager)
        {
            task->databasePager->start();
        }
    }
}

void ExperimentalViewer::assignRecordAndSubmitTaskAndPresentation(CommandGraphs commandGraphs, DatabasePager* databasePager)
{
    if (_windows.empty()) return;

    // TODO need to resolve what to do about graphic vs non graphic operations and mulitple windows
    Window* window = _windows.front();
    Device* device = window->device();
    PhysicalDevice* physicalDevice = window->physicalDevice();

    auto renderFinishedSemaphore = vsg::Semaphore::create(device);

    // set up Submission with CommandBuffer and signals
    auto recordAndSubmitTask = vsg::RecordAndSubmitTask::create();
    recordAndSubmitTask->commandGraphs = commandGraphs;
    recordAndSubmitTask->signalSemaphores.emplace_back(renderFinishedSemaphore);
    recordAndSubmitTask->databasePager = databasePager;
    recordAndSubmitTask->windows = _windows;
    recordAndSubmitTask->queue = device->getQueue(physicalDevice->getGraphicsFamily());
    recordAndSubmitTasks.emplace_back(recordAndSubmitTask);

    presentation = vsg::Presentation::create();
    presentation->waitSemaphores.emplace_back(renderFinishedSemaphore);
    presentation->windows = _windows;
    presentation->queue = device->getQueue(physicalDevice->getPresentFamily());
}


void ExperimentalViewer::update()
{
    for(auto& task: recordAndSubmitTasks)
    {
        if (task->databasePager)
        {
            task->databasePager->updateSceneGraph(_frameStamp);
        }
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
