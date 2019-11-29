#include <vsg/all.h>

namespace vsg
{
    // RecordAndSubmitTask
    class RecordAndSubmitTask : public Inherit<Object, RecordAndSubmitTask>
    {
    public:

        // Need to add FrameStamp?
        VkResult submit(ref_ptr<FrameStamp> frameStamp);

        using CommandGraphs = std::vector<ref_ptr<CommandGraph>>;

        Windows windows;
        Semaphores waitSemaphores; //
        CommandGraphs commandGraphs; // assign in application setup
        Semaphores signalSemaphores; // connect to Presentation.waitSemaphores

        ref_ptr<DatabasePager> databasePager;
        ref_ptr<Queue> queue; // assign in application for GraphicsQueue from device
    };

    class Presentation : public Inherit<Object, Presentation>
    {
    public:

        VkResult present();

        Windows windows;
        Semaphores waitSemaphores;  // taken from RecordAndSubmitTasks.signalSemaphores

        ref_ptr<Queue> queue; // assign in application for GraphicsQueue from device
    };


    class ExperimentalViewer : public Inherit<Viewer, ExperimentalViewer>
    {
    public:

        using RecordAndSubmitTasks = std::vector<ref_ptr<RecordAndSubmitTask>>;
        RecordAndSubmitTasks recordAndSubmitTasks;
        ref_ptr<Presentation> presentation;

        ExperimentalViewer();

        ~ExperimentalViewer();

        void addWindow(ref_ptr<Window> window);

        bool advanceToNextFrame() override;

        void advance() override;

        void handleEvents() override;

        void reassignFrameCache() override;

        bool acquireNextFrame() override;

        bool populateNextFrame() override;

        bool submitNextFrame() override;

        void compile(BufferPreferences bufferPreferences = {}) override;
    };
}
