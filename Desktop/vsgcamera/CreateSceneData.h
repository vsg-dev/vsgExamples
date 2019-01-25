#pragma once

#include <vsg/nodes/Node.h>

#include <vsg/vk/GraphicsPipeline.h>
#include <vsg/vk/PushConstants.h>
#include <vsg/vk/CommandPool.h>


extern vsg::ref_ptr<vsg::Node> createSceneData(vsg::Paths& searchPaths);
