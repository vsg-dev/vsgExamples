#pragma once

#include <vsg/core/Object.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>
#include <vsg/maths/quat.h>
#include <vsg/maths/vec3.h>
#include <vsg/ui/ApplicationEvent.h>
#include <vsg/ui/KeyEvent.h>
#include <vsg/utils/AnimationPath.h>
#include <vsg/viewer/Camera.h>

#include <map>

namespace vsg
{
    class AnimationPathHandler : public Inherit<Visitor, AnimationPathHandler>
    {
    public:
        AnimationPathHandler(ref_ptr<Camera> camera, ref_ptr<AnimationPath> animationPath, clock::time_point start_point);

        void apply(KeyPressEvent& keyPress) override;
        void apply(FrameEvent& frame) override;

    protected:
        ref_ptr<Camera> _camera;
        ref_ptr<LookAt> _lookAt;
        ref_ptr<AnimationPath> _path;
        KeySymbol _homeKey = KEY_Space;
        clock::time_point _start_point;
        unsigned int _frameCount = 0;
    };
} // namespace vsg
