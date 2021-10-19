#include "AnimationPath.h"

#include <vsg/maths/transform.h>

#include <iostream>

using namespace vsg;


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
    if (time > _path->period())
    {
        double average_framerate = double(_frameCount) / time;
        std::cout << "Period complete numFrames=" << _frameCount << ", average frame rate = " << average_framerate << std::endl;

        // reset time back to start
        _start_point = frame.frameStamp->time;
        time = 0.0;
        _frameCount = 0;
    }

    _lookAt->set(_path->computeMatrix(time));

    ++_frameCount;
}
