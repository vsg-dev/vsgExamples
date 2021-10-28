#pragma once
/* -*-c++-*-
*
*  OpenSceneGraph example, osgcluster.
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
*  THE SOFTWARE.
*/

////////////////////////////////////////////////////////////
// Receiver.h
//
// Class definition for the recipient of a broadcasted message
//

#if defined(_WIN32) && !defined(__CYGWIN__)
#    define NOMINMAX
#    include <winsock.h>
#else
#    include <netinet/in.h>
#endif

#include <vsg/core/Inherit.h>

class Receiver : public vsg::Inherit<vsg::Object, Receiver>
{
public:
    Receiver(uint16_t port);

    // Sync does a blocking wait to recieve next message
    unsigned int recieve(void* buffer, const unsigned int buffer_size);

private:
    bool init(void);

private:
    virtual ~Receiver();

#if defined(_WIN32) && !defined(__CYGWIN__)
    SOCKET _so;
    SOCKADDR_IN saddr;
#else
    int _so;
    struct sockaddr_in saddr;
#endif

    bool _initialized;
    short _port;
};
