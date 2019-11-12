#include <vsg/all.h>

namespace vsg
{
    class CommandGraph : public Inherit<Group, CommandGraph>
    {
    public:
        CommandGraph();

        void accept(DispatchTraversal& dispatchTraversal) const override;

        CommandBuffers commandBuffers; // assign one per index? Or just use round robin, each has a CommandPool
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
        ref_ptr<RenderPass> renderPass; // one per wndow
        Framebuffers framebuffers; // onr per swapchain image

        ref_ptr<Camera> camera; // camera that the trackball controls
        VkRect2D renderArea; // viewport dimensions
        ClearValues clearValues; // initialize window colour and depth/stencil
    };

    class Submission : public Inherit<Object, Submission>
    {
    public:

        // Need to add FrameStamp?
        VkResult submit(ref_ptr<FrameStamp> frameStamp);

        using CommandGraphs = std::vector<ref_ptr<CommandGraph>>;

        Semaphores waitSemaphores; //
        CommandGraphs commandGraphs; // assign in application setup
        Semaphores signalSemaphores; // connect to Presentation.waitSemaphores
        ref_ptr<Fence> fence; // assogn on first frame?

        ref_ptr<Queue> queue; // assign in application for GraphicsQueue from device
    };

    class Presentation : public Inherit<Object, Presentation>
    {
    public:

        VkResult present();

        using Swapchains = std::vector<ref_ptr<Swapchain>>;
        using Indices = std::vector<uint32_t>;

        Semaphores waitSemaphores;  // taken from Submissions.signalSemaphores
        Swapchains swapchains; // taaken from each window
        Indices indices; // taken from each window next frame index

        ref_ptr<Queue> queue; // assign in application for GraphicsQueue from device
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
