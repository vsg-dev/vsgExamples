#include <vsg/all.h>

namespace vsg
{
    class CommandGraph : public Inherit<Group, CommandGraph>
    {
    public:
        CommandGraph();

        void accept(DispatchTraversal& dispatchTraversal) const override;

        // do we buffer commanddBuffer and dispatchTraversal?
        // we need to buffer if existing commadnBuffer is still in use, i.e. fense associated with it hasn't been free yet.
        // do we need a CommandPool and buffer it? buffer per thread?
        // need to add FrameStamp
        ref_ptr<CommandBuffer> commandBuffer; // has a CommandPool
        ref_ptr<DispatchTraversal> dispatchTraversal;
    };

    using Framebuffers = std::vector<ref_ptr<Framebuffer>>;

    class RenderGraph : public Inherit<Group, RenderGraph>
    {
    public:

        RenderGraph();

        void accept(DispatchTraversal& dispatchTraversal) const override;

        using ClearValues = std::vector<VkClearValue>;

        // do we buffer frmaebuffer? One per swapchain image
        // do we need a handle to the assciated window to get the nextImageIndex?
        ref_ptr<RenderPass> renderPass;
        Framebuffers framebuffers;

        ref_ptr<Camera> camera;
        VkRect2D renderArea;
        ClearValues clearValues;
    };

    class Submission : public Inherit<Object, Submission>
    {
    public:

        // Need to add FrameStamp?
        VkResult submit(ref_ptr<FrameStamp> frameStamp);

        using CommandGraphs = std::vector<ref_ptr<CommandGraph>>;

        Semaphores waitSemaphores;
        CommandGraphs commandGraphs;
        Semaphores signalSemaphores;
        ref_ptr<Fence> fence;

        ref_ptr<Queue> queue;
    };

    class Presentation : public Inherit<Object, Presentation>
    {
    public:

        VkResult present();

        using Swapchains = std::vector<ref_ptr<Swapchain>>;
        using Indices = std::vector<uint32_t>;

        Semaphores waitSemaphores;
        Swapchains swapchains;
        Indices indices;

        ref_ptr<Queue> queue;
    };


    class ExperimentalViewer : public Inherit<Viewer, ExperimentalViewer>
    {
    public:

        using Submissions = std::vector<Submission>;
        Submissions submissions;
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
