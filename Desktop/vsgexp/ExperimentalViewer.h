#include <vsg/all.h>

namespace vsg
{
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

        // new
        void assignRecordAndSubmitTaskAndPresentation(CommandGraphs commandGraphs, DatabasePager* databasePager = nullptr);

        void compile(BufferPreferences bufferPreferences = {}) override;

        void update();

        void recordAndSubmit();

        void present();
    };
}
