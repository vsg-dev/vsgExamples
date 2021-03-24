/* <editor-fold desc="MIT License">

Copyright(c) 2019 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Options.h>
#include <vsg/viewer/Trackball.h>
#include <vsg/maths/transform.h>

#include "GlobeTrackball.h"

#include <iostream>

using namespace vsg;

GlobeTrackball::GlobeTrackball(ref_ptr<Camera> camera, ref_ptr<EllipsoidModel> ellipsoidModel) :
    Inherit(camera, ellipsoidModel)
{
    addKeyViewpoint(KEY_Space, LookAt::create(*_lookAt), 1.0);
}

void GlobeTrackball::apply(KeyPressEvent& keyPress)
{
    if (keyPress.handled || !_lastPointerEventWithinRenderArea) return;

    if (auto itr = keyViewpoitMap.find(keyPress.keyBase); itr != keyViewpoitMap.end())
    {
        _previousTime = keyPress.time;

        setViewpoint(itr->second.lookAt, itr->second.duration);

        keyPress.handled = true;
    }
}


void GlobeTrackball::apply(FrameEvent& frame)
{
    if (_endLookAt)
    {
        double timeSinceOfAnimation = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - _startTime).count();
        if (timeSinceOfAnimation < _animationDuration)
        {
            double r = smoothstep(0.0, 1.0, timeSinceOfAnimation / _animationDuration);

            if (_ellipsoidModel)
            {
                auto interpolate = [](const dvec3& start, const dvec3& end, double r) -> dvec3
                {
                    if (r >= 1.0) return end;

                    double length_start = length(start);
                    double length_end = length(end);
                    double acos_ratio = dot(start, end) / (length_start * length_end);
                    double angle = acos_ratio >= 1.0 ? 0.0 : (acos_ratio <= -1.0 ? vsg::PI : acos(acos_ratio));
                    auto cross_start_end = cross(start, end);
                    auto length_cross = length(cross_start_end);
                    if (angle != 0.0 && length_cross != 0.0)
                    {
                        cross_start_end /= length_cross;
                        auto rotation = vsg::rotate(angle * r, cross_start_end);
                        dvec3 new_dir =  normalize(rotation * start);
                        return new_dir * mix(length_start, length_end, r);
                    }
                    else
                    {
                        return mix(start, end, r);
                    }
                };

                auto interpolate_arc = [](const dvec3& start, const dvec3& end, double r, double arc_height = 0.0) -> dvec3
                {
                    if (r >= 1.0) return end;

                    double length_start = length(start);
                    double length_end = length(end);
                    double acos_ratio = dot(start, end) / (length_start * length_end);
                    double angle = acos_ratio >= 1.0 ? 0.0 : (acos_ratio <= -1.0 ? vsg::PI : acos(acos_ratio));
                    auto cross_start_end = cross(start, end);
                    auto length_cross = length(cross_start_end);
                    if (angle != 0.0 && length_cross != 0.0)
                    {
                        cross_start_end /= length_cross;
                        auto rotation = vsg::rotate(angle * r, cross_start_end);
                        dvec3 new_dir =  normalize(rotation * start);
                        double target_length = mix(length_start, length_end, r) + (r-r*r)*arc_height*4.0;
                        return new_dir * target_length;
                    }
                    else
                    {
                        return mix(start, end, r);
                    }
                };

                double length_center_start = length(_startLookAt->center);
                double length_center_end = length(_endLookAt->center);
                double length_center_mid = (length_center_start + length_center_end)*0.5;
                double distance_between = length(_startLookAt->center - _endLookAt->center);

                double transition_length = length_center_mid + distance_between;

                double length_eye_start = length(_startLookAt->eye);
                double length_eye_end = length(_endLookAt->eye);
                double length_eye_mid = (length_eye_start + length_eye_end)*0.5;

                double arc_height = (transition_length > length_eye_mid) ? (transition_length - length_eye_mid) : 0.0;

                _lookAt->eye = interpolate_arc(_startLookAt->eye, _endLookAt->eye, r, arc_height);
                _lookAt->center = interpolate(_startLookAt->center,_endLookAt->center, r);
                _lookAt->up = interpolate(_startLookAt->up, _endLookAt->up, r);
            }
            else
            {
                _lookAt->eye = mix(_startLookAt->eye, _endLookAt->eye, r);
                _lookAt->center = mix(_startLookAt->center,_endLookAt->center, r);

                double angle = acos(dot(_startLookAt->up, _endLookAt->up) / (length(_startLookAt->up) * length(_endLookAt->up)));
                if (angle != 0.0)
                {
                    auto rotation = vsg::rotate(angle * r, normalize(cross(_startLookAt->up, _endLookAt->up)));
                    _lookAt->up = rotation * _startLookAt->up;
                }
                else
                {
                    _lookAt->up = _endLookAt->up;
                }
            }


        }
        else
        {
            _lookAt->eye = _endLookAt->eye;
            _lookAt->center = _endLookAt->center;
            _lookAt->up = _endLookAt->up;

            _endLookAt = nullptr;
            _animationDuration = 0.0;
        }
    }
    else
    {
        Trackball::apply(frame);
    }
}

void GlobeTrackball::addKeyViewpoint(KeySymbol key, ref_ptr<LookAt> lookAt, double duration)
{
    keyViewpoitMap[key].lookAt = lookAt;
    keyViewpoitMap[key].duration = duration;
}

void GlobeTrackball::addKeyViewpoint(KeySymbol key, double latitude, double longitude, double altitude, double duration)
{
    if (!_ellipsoidModel) return;

    auto lookAt = LookAt::create();
    lookAt->eye = _ellipsoidModel->convertLatLongAltitudeToECEF(dvec3(latitude, longitude, altitude));
    lookAt->center = _ellipsoidModel->convertLatLongAltitudeToECEF(dvec3(latitude, longitude, 0.0));
    lookAt->up = normalize(cross(_lookAt->center, dvec3(-_lookAt->center.y, _lookAt->center.x, 0.0)));

    keyViewpoitMap[key].lookAt = lookAt;
    keyViewpoitMap[key].duration = duration;
}

void GlobeTrackball::setViewpoint(ref_ptr<LookAt> lookAt, double duration)
{
    if (!lookAt) return;

    _thrown = false;

    if (duration==0.0)
    {
        _lookAt->eye = lookAt->eye;
        _lookAt->center = lookAt->center;
        _lookAt->up = lookAt->up;

        _startLookAt = nullptr;
        _endLookAt = nullptr;
        _animationDuration = 0.0;

        clampToGlobe();
    }
    else
    {
        // EllipsoidModel
        //  rotation from original _lookAt->eye to new viewpoint.lookAt poition & start and end height
        //  rotation of _lookAt->center  to viewpoint.lookAt->center
        //  rototation of up vector between start and end points.
        //  rotate and scale.
        //
        // no EllipsoidModel
        // linear translation of eye and center, rotation of up.
        //
        // start_time & current_time vs duration
        // double ratio = (current_time-start_time)/duration
        //

        _startTime = _previousTime;
        _startLookAt = vsg::LookAt::create(*_lookAt);
        _endLookAt = lookAt;
        _animationDuration = duration;
    }
}
