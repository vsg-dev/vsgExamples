#include "AnimationPath.h"

#include <vsg/maths/transform.h>

#include <iostream>

using namespace vsg;

///////////////////////////////////////////////////////////////////////////////
//
// AnimationPath
//
AnimationPath::AnimationPath()
{
}

void AnimationPath::read(std::istream& fin)
{
    while (!fin.eof())
    {
        double time;
        dvec3 position;
        dquat rotation;
        fin >> time >> position.x >> position.y >> position.z >> rotation.x >> rotation.y >> rotation.z >> rotation.w;
        if (fin.good())
        {
            timeControlPointMap[time] = ControlPoint(position, rotation);
        }
    }
}

bool AnimationPath::getMatrix(double time, dmat4& matrix) const
{
    if (timeControlPointMap.empty()) return false;

    ControlPoint cp;
    auto itr = timeControlPointMap.lower_bound(time);
    if (itr == timeControlPointMap.begin())
    {
        //std::cout<<"At head time = "<<time<<", itr->first="<<itr->first<<std::endl;
        cp = itr->second;
    }
    else if (itr != timeControlPointMap.end())
    {
        auto previous_itr = itr;
        --previous_itr;

        double r = (time - previous_itr->first) / (itr->first - previous_itr->first);
        double one_minue_r = 1.0 - r;
        //std::cout<<"In middle = "<<time<<", previous_itr->first="<<previous_itr->first<<", itr->first="<<itr->first<<", r= "<<r<<std::endl;

        ControlPoint before = previous_itr->second;
        ControlPoint after = itr->second;

        cp.position = before.position * one_minue_r + after.position * r;
        cp.rotation = mix(before.rotation, after.rotation, r);
        cp.scale = before.scale * one_minue_r + after.scale * r;
    }
    else
    {
        //std::cout<<"At end time = "<<time<<", timeControlPointMap.rbegin()->->first="<<timeControlPointMap.rbegin()->first<<std::endl;
        cp = timeControlPointMap.rbegin()->second;
    }

#if 1

    matrix = translate(cp.position) * scale(cp.scale) * mat4_cast(cp.rotation);

#else
    matrix = translate(cp.position) * rotate(radians(90.0), dvec3(1.0, 0.0, 0.0));
#endif

    return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// AnimationPathHandler
//
AnimationPathHandler::AnimationPathHandler(ref_ptr<Camera> camera, ref_ptr<AnimationPath> animationPath, clock::time_point start_point) :
    _camera(camera),
    _path(animationPath),
    _start_point(start_point)
{
    _lookAt = _camera->viewMatrix.cast<LookAt>();
    if (!_lookAt)
    {
        // TODO: need to work out how to map the original ViewMatrix to a LookAt and back, for now just fallback to assigning our own LookAt
        _lookAt = LookAt::create();
    }
}

void AnimationPathHandler::apply(KeyPressEvent& keyPress)
{
    if (keyPress.keyBase == ' ')
    {
        _frameCount = 0;
    }
}

void AnimationPathHandler::apply(FrameEvent& frame)
{
    if (_frameCount == 0)
    {
        _start_point = frame.frameStamp->time;
    }

    double time = std::chrono::duration<double, std::chrono::seconds::period>(frame.frameStamp->time - _start_point).count();
    if (time > _path->getPeriod())
    {
        double average_framerate = double(_frameCount) / time;
        std::cout << "Period complete numFrames=" << _frameCount << ", average frame rate = " << average_framerate << std::endl;

        // reset time back to start
        _start_point = frame.frameStamp->time;
        time = 0.0;
        _frameCount = 0;
    }

    dmat4 matrix;
    _path->getMatrix(time, matrix);

    _lookAt->set(matrix);

    ++_frameCount;
}
