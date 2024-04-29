#pragma once

#include <vsg/app/RecordTraversal.h>
#include <vsg/core/Inherit.h>
#include <vsg/maths/vec4.h>
#include <vsg/vk/CommandBuffer.h>
#include <vsg/vk/vulkan.h>

#include <algorithm>
#include <string>

// A node which emits a debug label in the Vulkan command stream, which can be used by debugging
// utilities. For Example, RenderDoc will include these names in its event browser.

namespace experimental
{
    template<typename ParentClass>
    class Annotation : public vsg::Inherit<ParentClass, Annotation<ParentClass>>
    {
    public:
        template<typename... Args>
        Annotation(Args&&... args) :
            vsg::Inherit<ParentClass, Annotation<ParentClass>>(args...), debugColor(1.0f, 1.0f, 1.0f, 1.0f)
        {
        }

        vsg::vec4 debugColor;
        std::string annotation;

        using ParentClass::accept;

        void accept(vsg::RecordTraversal& visitor) const override
        {
            auto* commandBuffer = visitor.getCommandBuffer();
            auto extensions = commandBuffer->getDevice()->getInstance()->getExtensions();
            if (extensions->vkCmdBeginDebugUtilsLabelEXT && !annotation.empty())
            {
                VkCommandBuffer cmdBuffer{*commandBuffer};
                VkDebugUtilsLabelEXT markerInfo = {};
                markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
                markerInfo.pLabelName = annotation.c_str();
                std::copy(&debugColor.value[0], &debugColor.value[4], &markerInfo.color[0]);

                extensions->vkCmdBeginDebugUtilsLabelEXT(cmdBuffer, &markerInfo);

                ParentClass::accept(visitor);

                extensions->vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
            }
            else
            {
                ParentClass::accept(visitor);
            }
        }
    };
} // namespace experimental
