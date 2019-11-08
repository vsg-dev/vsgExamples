#include <vsg/all.h>

namespace vsg
{
    class CommandGraph : public Inherit<Group, CommandGraph>
    {
    public:
        ref_ptr<CommandBuffer> commandBuffer;
        ref_ptr<DispatchTraversal> dispatchTraversal;
    };

    class RenderGraph : public Inherit<Group, RenderGraph>
    {
    public:
        using ClearValues = std::vector<VkClearValue>;

        ref_ptr<RenderPass> renderPass;
        ref_ptr<Framebuffer> framebuffer;
        VkRect2D renderArea;
        ClearValues clearValues;
    };

    class Submission : public Inherit<Object, Submission>
    {
    public:

        VkResult submit();

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

        struct FrameObjects
        {
            ref_ptr<Submission> submission;
            ref_ptr<Presentation> presentation;
        };

        std::vector<FrameObjects> frameObjects;

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
