/**
 * MIT License

Copyright (c) [2025] [Nolan Kramer]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 * 
 */

#include "Turntable.h"

Turntable::Turntable(vsg::ref_ptr<vsg::Camera> camera) :
    _camera(camera),
    _lookAt(camera->viewMatrix.cast<vsg::LookAt>()),
    _keyboard(vsg::Keyboard::create())
{
    if (!_lookAt)
    {
        _lookAt = new vsg::LookAt;
    }
}

/// compute non dimensional window coordinate (-1,1) from event coords
vsg::dvec2 Turntable::ndc(const vsg::PointerEvent& event)
{
    auto renderArea = _camera->getRenderArea();
    auto [x, y] = cameraRenderAreaCoordinates(event);

    double aspectRatio = static_cast<double>(renderArea.extent.width) / static_cast<double>(renderArea.extent.height);
    vsg::dvec2 v(
        (renderArea.extent.width > 0) ? (static_cast<double>(x - renderArea.offset.x) / static_cast<double>(renderArea.extent.width) * 2.0 - 1.0) * aspectRatio : 0.0,
        (renderArea.extent.height > 0) ? static_cast<double>(y - renderArea.offset.y) / static_cast<double>(renderArea.extent.height) * 2.0 - 1.0 : 0.0);
    return v;
}

/// compute turntable coordinate from event coords
vsg::dvec3 Turntable::ttc(const vsg::PointerEvent& event)
{
    vsg::dvec2 v = ndc(event);

    // Compute the z coordinate for projection onto a sphere
    double x2 = v.x * v.x;
    double y2 = v.y * v.y;
    double r2 = x2 + y2;

    double z;
    if (r2 < 0.5) // Inside sphere
    {
        z = sqrt(1.0 - r2);
    } else { // On hyperbola
        z = 0.5 / sqrt(r2);
    }

    return vsg::dvec3(v.x, v.y, z);
}

void Turntable::apply(vsg::KeyPressEvent& keyPress)
{
    if (_keyboard) keyPress.accept(*_keyboard);

    if (!_hasKeyboardFocus || keyPress.handled || !eventRelevant(keyPress)) return;

    if (auto itr = keyViewpointMap.find(keyPress.keyBase); itr != keyViewpointMap.end())
    {
        _previousTime = keyPress.time;

        setViewpoint(itr->second.lookAt, itr->second.duration);

        keyPress.handled = true;
    }
}

void Turntable::apply(vsg::KeyReleaseEvent& keyRelease)
{
    if (_keyboard) keyRelease.accept(*_keyboard);
}

void Turntable::apply(vsg::FocusInEvent& focusIn)
{
    if (_keyboard) focusIn.accept(*_keyboard);
}

void Turntable::apply(vsg::FocusOutEvent& focusOut)
{
    if (_keyboard) focusOut.accept(*_keyboard);
}

void Turntable::apply(vsg::ButtonPressEvent& buttonPress)
{
    if (buttonPress.handled || !eventRelevant(buttonPress))
    {
        _hasKeyboardFocus = false;
        return;
    }

    _hasPointerFocus = _hasKeyboardFocus = withinRenderArea(buttonPress);
    _lastPointerEventWithinRenderArea = _hasPointerFocus;

    if (buttonPress.mask & rotateButtonMask)
        _updateMode = ROTATE;
    else if (buttonPress.mask & panButtonMask)
        _updateMode = PAN;
    else if (buttonPress.mask & zoomButtonMask)
        _updateMode = ZOOM;
    else
        _updateMode = INACTIVE;

    if (_hasPointerFocus) buttonPress.handled = true;

    _zoomPreviousRatio = 0.0;
    _pan.set(0.0, 0.0);
    _rotateAngle = 0.0;

    _previousPointerEvent = &buttonPress;
}

void Turntable::apply(vsg::ButtonReleaseEvent& buttonRelease)
{
    if (buttonRelease.handled || !eventRelevant(buttonRelease)) return;

    if (!windowOffsets.empty() && windowOffsets.count(buttonRelease.window) == 0) return;

    if (supportsThrow) _thrown = _previousPointerEvent && (buttonRelease.time == _previousPointerEvent->time);

    _lastPointerEventWithinRenderArea = withinRenderArea(buttonRelease);
    _hasPointerFocus = false;

    _previousPointerEvent = &buttonRelease;
}

void Turntable::apply(vsg::MoveEvent& moveEvent)
{
    if (!eventRelevant(moveEvent)) return;

    _lastPointerEventWithinRenderArea = withinRenderArea(moveEvent);

    if (moveEvent.handled || !_hasPointerFocus) return;

    if (!_previousPointerEvent) _previousPointerEvent = &moveEvent;

    vsg::dvec2 new_ndc = ndc(moveEvent);
    vsg::dvec2 prev_ndc = ndc(*_previousPointerEvent);
    vsg::dvec2 control_ndc = new_ndc;

    double dt = std::chrono::duration<double, std::chrono::seconds::period>(moveEvent.time - _previousPointerEvent->time).count();
    _previousDelta = dt;

    double scale = 1.0;
    //if (_previousTime > _previousPointerEvent->time) scale = std::chrono::duration<double, std::chrono::seconds::period>(moveEvent.time - _previousTime).count() / dt;
    //    scale *= 2.0;

    _previousTime = moveEvent.time;
    
    if (moveEvent.mask & rotateButtonMask)
    {
        _updateMode = ROTATE;

        moveEvent.handled = true;
        
        // Calculate deltas for horizontal and vertical rotations
        vsg::dvec2 delta = control_ndc - prev_ndc;
        
        double rotationFactor = 1.0;
        if (delta.x != 0.0)
        {
            // Horizontal rotation around the up axis (Z)
            double rotateAngle = -delta.x * rotationFactor;
            vsg::dvec3 rotateAxis(0.0, 0.0, 1.0);
            rotate(rotateAngle * scale, rotateAxis);
        }
        
        if (delta.y != 0.0)
        {
            // Vertical rotation around the local side vector
            vsg::dvec3 lookVector = vsg::normalize(_lookAt->center - _lookAt->eye);
            vsg::dvec3 sideVector = vsg::normalize(vsg::cross(lookVector, _lookAt->up));

            // The negative sign makes upward mouse movement tilt the camera up
            double rotateAngle = -delta.y * rotationFactor;
            rotate(rotateAngle * scale, sideVector);
        }
    }
    else if (moveEvent.mask & panButtonMask)
    {
        _updateMode = PAN;
        moveEvent.handled = true;
        vsg::dvec2 delta = control_ndc - prev_ndc;
        _pan = delta;
        pan(delta * scale);
    }
    else if (moveEvent.mask & zoomButtonMask)
    {
        _updateMode = ZOOM;
        moveEvent.handled = true;
        vsg::dvec2 delta = control_ndc - prev_ndc;
        if (delta.y != 0.0)
        {
            _zoomPreviousRatio = zoomScale * 2.0 * delta.y;
            zoom(_zoomPreviousRatio * scale);
        }
    }
    
    _thrown = false;
    _previousPointerEvent = &moveEvent;
}

void Turntable::apply(vsg::ScrollWheelEvent& scrollWheel)
{
    if (scrollWheel.handled || !eventRelevant(scrollWheel) || !_lastPointerEventWithinRenderArea) return;

    scrollWheel.handled = true;

    zoom(scrollWheel.delta.y * 0.1);
}

void Turntable::apply(vsg::TouchDownEvent& touchDown)
{
    if (!eventRelevant(touchDown)) return;

    _previousTouches[touchDown.id] = &touchDown;
    switch (touchDown.id)
    {
    case 0: {
        if (_previousTouches.size() == 1)
        {
            vsg::ref_ptr<vsg::Window> w = touchDown.window;
            vsg::ref_ptr<vsg::ButtonPressEvent> evt = vsg::ButtonPressEvent::create(
                w,
                touchDown.time,
                touchDown.x,
                touchDown.y,
                touchMappedToButtonMask,
                touchDown.id);
            apply(*evt.get());
        }
        break;
    }
    case 1: {
        _prevZoomTouchDistance = 0.0;
        if (touchDown.id == 0 && _previousTouches.count(1))
        {
            const auto& prevTouch1 = _previousTouches[1];
            auto a = std::abs(static_cast<double>(prevTouch1->x) - touchDown.x);
            auto b = std::abs(static_cast<double>(prevTouch1->y) - touchDown.y);
            if (a > 0 || b > 0)
                _prevZoomTouchDistance = sqrt(a * a + b * b);
        }
        break;
    }
    }
}

void Turntable::apply(vsg::TouchUpEvent& touchUp)
{
    if (!eventRelevant(touchUp)) return;

    if (touchUp.id == 0 && _previousTouches.size() == 1)
    {
        vsg::ref_ptr<vsg::Window> w = touchUp.window;
        vsg::ref_ptr<vsg::ButtonReleaseEvent> evt = vsg::ButtonReleaseEvent::create(
            w,
            touchUp.time,
            touchUp.x,
            touchUp.y,
            touchMappedToButtonMask,
            touchUp.id);
        apply(*evt.get());
    }
    _previousTouches.erase(touchUp.id);
}

void Turntable::apply(vsg::TouchMoveEvent& touchMove)
{
    if (!eventRelevant(touchMove)) return;

    vsg::ref_ptr<vsg::Window> w = touchMove.window;
    switch (_previousTouches.size())
    {
    case 1: {
        // Rotate
        vsg::ref_ptr<vsg::MoveEvent> evt = vsg::MoveEvent::create(
            w,
            touchMove.time,
            touchMove.x,
            touchMove.y,
            touchMappedToButtonMask);
        apply(*evt.get());
        break;
    }
    case 2: {
        if (touchMove.id == 0 && _previousTouches.count(0))
        {
            // Zoom
            const auto& prevTouch1 = _previousTouches[1];
            auto a = std::abs(static_cast<double>(prevTouch1->x) - touchMove.x);
            auto b = std::abs(static_cast<double>(prevTouch1->y) - touchMove.y);
            if (a > 0 || b > 0)
            {
                auto touchZoomDistance = sqrt(a * a + b * b);
                if (_prevZoomTouchDistance && touchZoomDistance > 0)
                {
                    auto zoomLevel = touchZoomDistance / _prevZoomTouchDistance;
                    if (zoomLevel < 1)
                        zoomLevel = -(1 / zoomLevel);
                    zoomLevel *= 0.1;
                    zoom(zoomLevel);
                }
                _prevZoomTouchDistance = touchZoomDistance;
            }
        }
        break;
    }
    }
    _previousTouches[touchMove.id] = &touchMove;
}

void Turntable::apply(vsg::FrameEvent& frame)
{
    // Handle keyboard input when focused
    if (_hasKeyboardFocus && _keyboard)
    {
        auto times2speed = [](std::pair<double, double> duration) -> double {
            if (duration.first <= 0.0) return 0.0;
            double speed = duration.first >= 1.0 ? 1.0 : duration.first;

            if (duration.second > 0.0)
            {
                // key has been released so slow down
                speed -= duration.second;
                return speed > 0.0 ? speed : 0.0;
            }
            else
            {
                // key still pressed so return speed based on duration of press
                return speed;
            }
        };

        double speed = 0.0;
        vsg::dvec3 move(0.0, 0.0, 0.0);
        if ((speed = times2speed(_keyboard->times(moveLeftKey))) != 0.0) move.x += -speed;
        if ((speed = times2speed(_keyboard->times(moveRightKey))) != 0.0) move.x += speed;
        if ((speed = times2speed(_keyboard->times(moveUpKey))) != 0.0) move.y += speed;
        if ((speed = times2speed(_keyboard->times(moveDownKey))) != 0.0) move.y += -speed;
        if ((speed = times2speed(_keyboard->times(moveForwardKey))) != 0.0) move.z += speed;
        if ((speed = times2speed(_keyboard->times(moveBackwardKey))) != 0.0) move.z += -speed;

        vsg::dvec3 rot(0.0, 0.0, 0.0);
        // For turntable, we primarily care about the horizontal rotation (around up vector)
        if ((speed = times2speed(_keyboard->times(turnLeftKey))) != 0.0) rot.x += speed;
        if ((speed = times2speed(_keyboard->times(turnRightKey))) != 0.0) rot.x -= speed;
        // Vertical rotation (tilt)
        if ((speed = times2speed(_keyboard->times(pitchUpKey))) != 0.0) rot.y += speed;
        if ((speed = times2speed(_keyboard->times(pitchDownKey))) != 0.0) rot.y -= speed;
        // Typically turntable doesn't have roll, but keeping minimal functionality
        if ((speed = times2speed(_keyboard->times(rollLeftKey))) != 0.0) rot.z -= speed * 0.2; // Reduced influence
        if ((speed = times2speed(_keyboard->times(rollRightKey))) != 0.0) rot.z += speed * 0.2; // Reduced influence

        if (rot || move)
        {
            double scale = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - _previousTime).count();
            double scaleTranslation = scale * 0.2 * vsg::length(_lookAt->center - _lookAt->eye);
            double scaleRotation = scale * 0.5;

            vsg::dvec3 upVector = _lookAt->up;
            vsg::dvec3 lookVector = vsg::normalize(_lookAt->center - _lookAt->eye);
            vsg::dvec3 sideVector = vsg::normalize(vsg::cross(lookVector, upVector));

            // For turntable, we primarily translate in the horizontal plane
            vsg::dvec3 horizontalMove = sideVector * (scaleTranslation * move.x) + 
                                   vsg::cross(upVector, sideVector) * (scaleTranslation * move.z);
            // Vertical movement is still allowed
            vsg::dvec3 verticalMove = upVector * (scaleTranslation * move.y);
            vsg::dvec3 delta = horizontalMove + verticalMove;

            // Store the original distance for maintaining orbit radius
            // double distanceFromCenter = vsg::length(_lookAt->eye - _lookAt->center);

            // Get the global up vector (assuming Z-up, change if needed)
            vsg::dvec3 globalUp(0.0, 0.0, 1.0);

            // Apply rotation with gimbal lock avoidance
            applyRotationWithGimbalAvoidance(rot, scaleRotation, globalUp);

            // Apply translation
            _lookAt->eye += delta;
            _lookAt->center += delta;

            _thrown = false;
        }
    }

    if (_endLookAt)
    {
        double timeSinceOfAnimation = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - _startTime).count();
        if (timeSinceOfAnimation < _animationDuration)
        {
            double r = vsg::smoothstep(0.0, 1.0, timeSinceOfAnimation / _animationDuration);

            _lookAt->eye = vsg::mix(_startLookAt->eye, _endLookAt->eye, r);
            _lookAt->center = vsg::mix(_startLookAt->center, _endLookAt->center, r);

            double angle = acos(vsg::dot(_startLookAt->up, _endLookAt->up) / (vsg::length(_startLookAt->up) * vsg::length(_endLookAt->up)));
            if (angle > 1.0e-6)
            {
                auto rotation = vsg::rotate(angle * r, vsg::normalize(vsg::cross(_startLookAt->up, _endLookAt->up)));
                _lookAt->up = rotation * _startLookAt->up;
            }
            else
            {
                _lookAt->up = _endLookAt->up;
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
    else if (_thrown)
    {
        double scale = _previousDelta > 0.0 ? std::chrono::duration<double, std::chrono::seconds::period>(frame.time - _previousTime).count() / _previousDelta : 0.0;
        switch (_updateMode)
        {
        case (ROTATE):
            // For turntable thrown rotation, avoid gimbal lock
            {
                vsg::dvec3 globalUp(0.0, 0.0, 1.0); // Assuming Z-up
                vsg::dvec3 lookVector = vsg::normalize(_lookAt->center - _lookAt->eye);
                
                // Decompose rotation into horizontal and vertical components
                double verticalComponent = vsg::dot(_rotateAxis, globalUp);
                
                // Create rotation vectors that will help avoid gimbal lock
                vsg::dvec3 horizontalAxis, verticalAxis;
                computeRotationAxesWithGimbalAvoidance(horizontalAxis, verticalAxis, lookVector, globalUp);
                
                // Apply horizontal rotation
                double horizontalAngle = _rotateAngle * (1.0 - std::abs(verticalComponent)) * scale;
                rotate(horizontalAngle, horizontalAxis);
                
                // Apply vertical rotation 
                double verticalAngle = _rotateAngle * verticalComponent * scale;
                rotate(verticalAngle, verticalAxis);
            }
            break;
        case (PAN):
            // For turntable, we primarily pan in the horizontal plane
            {
                vsg::dvec3 globalUp(0.0, 0.0, 1.0);
                vsg::dvec3 pan3D(_pan.x, _pan.y, 0.0);
                vsg::dvec3 horizontalPan = pan3D - globalUp * vsg::dot(pan3D, globalUp);
                auto scaled = horizontalPan * scale + globalUp * vsg::dot(pan3D, globalUp) * scale * 0.5;
                pan(vsg::dvec2(scaled.x, scaled.y));
            }
            break;
        case (ZOOM):
            zoom(_zoomPreviousRatio * scale);
            break;
        default:
            break;
        }
    }

    _previousTime = frame.time;
}

// Method implementing gimbal lock avoidance for turntable controls
void Turntable::applyRotationWithGimbalAvoidance(const vsg::dvec3& rot, double scaleRotation, const vsg::dvec3& globalUp)
{
    // Get current view vectors
    vsg::dvec3 lookVector = vsg::normalize(_lookAt->center - _lookAt->eye);
    
    // Compute the rotation axes with gimbal lock avoidance
    vsg::dvec3 horizontalAxis, verticalAxis;
    computeRotationAxesWithGimbalAvoidance(horizontalAxis, verticalAxis, lookVector, globalUp);
    
    // Apply horizontal rotation (around a modified up vector that avoids gimbal lock)
    double horizontalAngle = rot.x * scaleRotation;
    rotate(horizontalAngle, horizontalAxis);
    
    // Apply vertical rotation (around the side vector)
    double verticalAngle = rot.y * scaleRotation;
    rotate(verticalAngle, verticalAxis);
    
    // Apply minimal roll if specified
    if (rot.z != 0.0)
    {
        double rollAngle = rot.z * scaleRotation;
        rotate(rollAngle, lookVector);
    }
}

// Implementation of gimbal lock avoidance algorithm
void Turntable::computeRotationAxesWithGimbalAvoidance(vsg::dvec3& horizontalAxis, vsg::dvec3& verticalAxis, 
                                                      const vsg::dvec3& lookVector, const vsg::dvec3& globalUp)
{
    // 1. Compute how close we are to gimbal lock
    double viewVerticalAlignment = std::abs(vsg::dot(lookVector, globalUp));
    
    // The closer viewVerticalAlignment is to 1.0, the closer we are to gimbal lock
    
    // 2. Compute the rotation axes:
    
    // 2.1. Standard turntable axes
    vsg::dvec3 standardUp = globalUp;  // Standard up vector for horizontal rotation
    vsg::dvec3 standardSide = vsg::normalize(vsg::cross(lookVector, standardUp)); // Standard side vector for vertical rotation
    
    // 2.2. Rotated axes
    // Cross product of global up and look direction gives us an axis perpendicular to both
    vsg::dvec3 rotatedSide = vsg::normalize(vsg::cross(globalUp, lookVector));
    // Another cross product gives us a "rotated horizon" axis
    vsg::dvec3 rotatedUp = vsg::normalize(vsg::cross(rotatedSide, lookVector));
    
    // 3. Blend between the standard and rotated axes based on gimbal lock severity
    
    // This blending factor increases as we approach gimbal lock
    // Using a smooth curve that increases more rapidly as we approach gimbal lock
    double blendFactor = vsg::smoothstep(0.7, 0.98, viewVerticalAlignment);
    
    // Blend the up vectors (horizontal rotation axis)
    horizontalAxis = vsg::normalize(vsg::mix(standardUp, rotatedUp, blendFactor));
    
    // For vertical rotation, we blend between standard side vector and a computed side vector
    // that's based on our current blended up vector (to ensure orthogonality)
    vsg::dvec3 blendedSide = vsg::normalize(vsg::cross(lookVector, horizontalAxis));
    verticalAxis = vsg::normalize(vsg::mix(standardSide, blendedSide, blendFactor));
    
    // Ensure axes are normalized
    horizontalAxis = vsg::normalize(horizontalAxis);
    verticalAxis = vsg::normalize(verticalAxis);
}

void Turntable::rotate(double angle, const vsg::dvec3& axis)
{
    if (std::abs(angle) < 1e-10 || vsg::length(axis) < 1e-10)
        return;
        
    // Create rotation matrix around the provided axis
    vsg::dmat4 matrix = vsg::translate(_lookAt->center) *
                   vsg::rotate(angle, vsg::normalize(axis)) *
                   vsg::translate(-_lookAt->center);
                   
    // Apply rotation
    _lookAt->eye = matrix * _lookAt->eye;
    _lookAt->up = vsg::normalize(matrix * (_lookAt->eye + _lookAt->up) - matrix * _lookAt->eye);
}

void Turntable::zoom(double ratio)
{
    vsg::dvec3 lookVector = _lookAt->center - _lookAt->eye;
    _lookAt->eye = _lookAt->eye + lookVector * ratio;
}

void Turntable::pan(const vsg::dvec2& delta)
{
    vsg::dvec3 lookVector = _lookAt->center - _lookAt->eye;
    vsg::dvec3 lookNormal = normalize(lookVector);
    vsg::dvec3 upNormal = _lookAt->up;
    vsg::dvec3 sideNormal = cross(lookNormal, upNormal);

    double distance = vsg::length(lookVector);
    distance *= 0.25;

    vsg::dvec3 translation = sideNormal * (-delta.x * distance) + upNormal * (delta.y * distance);

    _lookAt->eye = _lookAt->eye + translation;
    _lookAt->center = _lookAt->center + translation;
}

std::pair<int32_t, int32_t> Turntable::cameraRenderAreaCoordinates(const vsg::PointerEvent& pointerEvent) const
{
    if (!pointerEvent.window) return {0, 0};
    
    auto itr = windowOffsets.find(pointerEvent.window);
    if (itr != windowOffsets.end())
    {
        auto coords = itr->second;
        return {pointerEvent.x + coords.x, pointerEvent.y + coords.y};
    }
    
    return {pointerEvent.x, pointerEvent.y};
}

bool Turntable::withinRenderArea(const vsg::PointerEvent& pointerEvent) const
{
    if (!_camera || !_camera->viewportState) return false;
    
    auto [x, y] = cameraRenderAreaCoordinates(pointerEvent);
    
    const auto& viewport = _camera->viewportState->getViewport();
    return (x >= viewport.x && x < viewport.x + viewport.width &&
            y >= viewport.y && y < viewport.y + viewport.height);
}

bool Turntable::eventRelevant(const vsg::WindowEvent& event) const
{
    if (windowOffsets.empty()) return true;

    return (windowOffsets.count(event.window) > 0);
}

void Turntable::addWindow(vsg::ref_ptr<vsg::Window> window, const vsg::ivec2& offset)
{
    windowOffsets[vsg::observer_ptr<vsg::Window>(window)] = offset;
}

void Turntable::addKeyViewpoint(vsg::KeySymbol key, vsg::ref_ptr<vsg::LookAt> lookAt, double duration)
{
    keyViewpointMap[key].lookAt = lookAt;
    keyViewpointMap[key].duration = duration;
}

void Turntable::setViewpoint(vsg::ref_ptr<vsg::LookAt> lookAt, double duration)
{
    if (!lookAt) return;

    _thrown = false;

    if (duration == 0.0)
    {
        _lookAt->eye = lookAt->eye;
        _lookAt->center = lookAt->center;
        _lookAt->up = lookAt->up;

        _startLookAt = nullptr;
        _endLookAt = nullptr;
        _animationDuration = 0.0;
    } else {
        _startTime = _previousTime;
        _startLookAt = vsg::LookAt::create(*_lookAt);
        _endLookAt = lookAt;
        _animationDuration = duration;
    }
}
