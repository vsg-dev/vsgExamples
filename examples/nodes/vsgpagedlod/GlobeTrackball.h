#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2019 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/viewer/Trackball.h>


namespace vsg
{
    class VSG_DECLSPEC GlobeTrackball : public Inherit<Trackball, GlobeTrackball>
    {
    public:
        GlobeTrackball(ref_ptr<Camera> camera, ref_ptr<EllipsoidModel> ellipsoidModel = {});

        void apply(KeyPressEvent& keyPress) override;
        void apply(FrameEvent& frame) override;

        void addKeyViewpoint(KeySymbol key, ref_ptr<LookAt> lookAt, double duration = 1.0);
        void addKeyViewpoint(KeySymbol key, double latitude, double longitude, double altitude, double duration = 1.0);

        void setViewpoint(ref_ptr<LookAt> lookAt, double duration = 1.0);

        struct Viewpoint
        {
            ref_ptr<LookAt> lookAt;
            double duration = 0.0;
        };

        std::map<KeySymbol, Viewpoint> keyViewpoitMap;

    protected:

        time_point _startTime;
        ref_ptr<LookAt> _startLookAt;
        ref_ptr<LookAt> _endLookAt;
        double _animationDuration = 0.0;
    };

} // namespace vsg
