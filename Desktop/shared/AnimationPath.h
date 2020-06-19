#pragma once

#include <vsg/core/Object.h>
#include <vsg/maths/vec3.h>
#include <vsg/maths/vec3.h>
#include <vsg/maths/quat.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>
#include <vsg/ui/KeyEvent.h>
#include <vsg/ui/ApplicationEvent.h>
#include <vsg/viewer/Camera.h>

#include <map>

namespace vsg
{

    class AnimationPath : public Object
    {
    public:

        AnimationPath();

        void read(std::istream& fin);

    #if 0
        void read(Input& input) override;
        void write(Output& output) override;
    #endif

        struct ControlPoint
        {
            ControlPoint():
                position(0.0, 0.0, 0.0),
                rotation(0.0, 0.0, 0.0, 1.0),
                scale(1.0, 1.0, 1.0) {}

            ControlPoint(const dvec3& p, const dquat& r):
                position(p),
                rotation(r),
                scale(1.0, 1.0, 1.0) {}

            ControlPoint(const dvec3& p, const dquat& r, const dvec3& s):
                position(p),
                rotation(r),
                scale(s) {}

            dvec3 position;
            dquat rotation;
            dvec3 scale;
        };

        double getPeriod() const { return timeControlPointMap.empty() ? 0.0 : (timeControlPointMap.rbegin()->first - timeControlPointMap.begin()->first); }

        bool getMatrix(double time, dmat4& matrix) const;

        using TimeControlPointMap = std::map<double, ControlPoint>;
        TimeControlPointMap timeControlPointMap;
    };

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
}
