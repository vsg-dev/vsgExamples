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

#pragma once

#include <vsg/all.h>

class Turntable : public vsg::Inherit<vsg::Visitor, Turntable>
{
    public:
        explicit Turntable(vsg::ref_ptr<vsg::Camera> camera);

        /// compute non dimensional window coordinate (-1,1) from event coords
        vsg::dvec2 ndc(const vsg::PointerEvent& event);

        /// compute turntable coordinate from event coords
        vsg::dvec3 ttc(const vsg::PointerEvent& event);

        void apply(vsg::KeyPressEvent& keyPress) override;
        void apply(vsg::KeyReleaseEvent& keyRelease) override;
        void apply(vsg::FocusInEvent& focusIn) override;
        void apply(vsg::FocusOutEvent& focusOut) override;
        void apply(vsg::ButtonPressEvent& buttonPress) override;
        void apply(vsg::ButtonReleaseEvent& buttonRelease) override;
        void apply(vsg::MoveEvent& moveEvent) override;
        void apply(vsg::ScrollWheelEvent& scrollWheel) override;
        void apply(vsg::TouchDownEvent& touchDown) override;
        void apply(vsg::TouchUpEvent& touchUp) override;
        void apply(vsg::TouchMoveEvent& touchMove) override;
        void apply(vsg::FrameEvent& frame) override;

        virtual void rotate(double angle, const vsg::dvec3& axis);
        virtual void zoom(double ratio);
        virtual void pan(const vsg::dvec2& delta);

        std::pair<int32_t, int32_t> cameraRenderAreaCoordinates(const vsg::PointerEvent& pointerEvent) const;
        bool withinRenderArea(const vsg::PointerEvent& pointerEvent) const;
        bool eventRelevant(const vsg::WindowEvent& event) const;

        /// list of windows that this Trackball should respond to events from, and the points xy offsets to apply
        std::map<vsg::observer_ptr<vsg::Window>, vsg::ivec2> windowOffsets;

        /// add a Window to respond events for, with mouse coordinate offset to treat all associated windows
        void addWindow(vsg::ref_ptr<vsg::Window> window, const vsg::ivec2& offset = {});

        /// add Key to Viewpoint binding using a LookAt to define the viewpoint
        void addKeyViewpoint(vsg::KeySymbol key, vsg::ref_ptr<vsg::LookAt> lookAt, double duration = 1.0);

        /// set the LookAt viewport to the specified lookAt, animating the movements from the current lookAt to the new one.
        /// A value of 0.0 instantly moves the lookAt to the new value.
        void setViewpoint(vsg::ref_ptr<vsg::LookAt> lookAt, double duration = 1.0);

        struct Viewpoint
        {
            vsg::ref_ptr<vsg::LookAt> lookAt;
            double duration = 0.0;
        };

        /// container that maps key symbol bindings with the Viewpoint that should move the LookAt to when pressed.
        std::map<vsg::KeySymbol, Viewpoint> keyViewpointMap;

        /// Key that turns the view left around the eye points
        vsg::KeySymbol turnLeftKey = vsg::KEY_a;

        /// Key that turns the view right around the eye points
        vsg::KeySymbol turnRightKey = vsg::KEY_d;

        /// Key that pitches up the view around the eye point
        vsg::KeySymbol pitchUpKey = vsg::KEY_w;

        /// Key that pitches down the view around the eye point
        vsg::KeySymbol pitchDownKey = vsg::KEY_s;

        /// Key that rools the view anti-clockwise/left
        vsg::KeySymbol rollLeftKey = vsg::KEY_q;

        /// Key that rolls the view clockwise/right
        vsg::KeySymbol rollRightKey = vsg::KEY_e;

        /// Key that moves the view forward
        vsg::KeySymbol moveForwardKey = vsg::KEY_o;

        /// Key that moves the view backwards
        vsg::KeySymbol moveBackwardKey = vsg::KEY_i;

        /// Key that moves the view left
        vsg::KeySymbol moveLeftKey = vsg::KEY_Left;

        /// Key that moves the view right
        vsg::KeySymbol moveRightKey = vsg::KEY_Right;

        /// Key that moves the view upward
        vsg::KeySymbol moveUpKey = vsg::KEY_Up;

        /// Key that moves the view downward
        vsg::KeySymbol moveDownKey = vsg::KEY_Down;

        /// Button mask value used to enable rotating of the view, defaults to left mouse button
        vsg::ButtonMask rotateButtonMask = vsg::BUTTON_MASK_1;

        /// Button mask value used to enable panning of the view, defaults to middle mouse button
        vsg::ButtonMask panButtonMask = vsg::BUTTON_MASK_2;

        /// Button mask value used to enable zooming of the view, defaults to right mouse button
        vsg::ButtonMask zoomButtonMask = vsg::BUTTON_MASK_3;

        /// Button mask value used used for touch events
        vsg::ButtonMask touchMappedToButtonMask = vsg::BUTTON_MASK_1;

        /// Scale for controlling how rapidly the view zooms in/out. Positive value zooms in when mouse moves downwards
        double zoomScale = 1.0;

        /// Toggle on/off whether the view should continue moving when the mouse buttons are released while the mouse is in motion.
        bool supportsThrow = true;

    protected:
        vsg::ref_ptr<vsg::Camera> _camera;
        vsg::ref_ptr<vsg::LookAt> _lookAt;

        bool _hasKeyboardFocus = false;
        bool _hasPointerFocus = false;
        bool _lastPointerEventWithinRenderArea = false;

        enum UpdateMode
        {
            INACTIVE = 0,
            ROTATE,
            PAN,
            ZOOM
        };
        UpdateMode _updateMode = INACTIVE;
        double _zoomPreviousRatio = 0.0;
        vsg::dvec2 _pan;
        double _rotateAngle = 0.0;
        vsg::dvec3 _rotateAxis;

        vsg::time_point _previousTime;
        vsg::ref_ptr<vsg::PointerEvent> _previousPointerEvent;
        double _previousDelta = 0.0;
        double _prevZoomTouchDistance = 0.0;
        bool _thrown = false;

        vsg::time_point _startTime;
        vsg::ref_ptr<vsg::LookAt> _startLookAt;
        vsg::ref_ptr<vsg::LookAt> _endLookAt;
        std::map<uint32_t, vsg::ref_ptr<vsg::TouchEvent>> _previousTouches;

        vsg::ref_ptr<vsg::Keyboard> _keyboard;

        double _animationDuration = 0.0;

        void applyRotationWithGimbalAvoidance(const vsg::dvec3& rot, double scaleRotation, const vsg::dvec3& globalUp);
        void computeRotationAxesWithGimbalAvoidance(vsg::dvec3& horizontalAxis, vsg::dvec3& verticalAxis, 
            const vsg::dvec3& lookVector, const vsg::dvec3& globalUp);
};