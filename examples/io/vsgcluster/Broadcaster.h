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

#include <string>
#include <vsg/core/Inherit.h>

////////////////////////////////////////////////////////////
// Broadcaster.h
//
// Class definition for broadcasting a buffer to a LAN
//

#if defined(_WIN32) && !defined(__CYGWIN__)
#    define NOMINMAX
#    include <winsock.h>
#else
#    include <netinet/in.h>
#endif

std::vector<std::string> listNetworkConnections();

class Broadcaster : public vsg::Inherit<vsg::Object, Broadcaster>
{
public:
    Broadcaster(uint16_t port, const std::string& ifrName = {});

    Broadcaster(const std::string& hostname, uint16_t port, const std::string& ifrName = {});

    // Set the buffer to be broadcast
    void setBuffer();

    void broadcast(const void* buffer, unsigned int buffer_size);

private:
    bool init(void);

private:
    virtual ~Broadcaster();

    std::string _ifr_name;

#if defined(_WIN32) && !defined(__CYGWIN__)
    SOCKET _so;
#else
    int _so;
#endif
    bool _initialized;
    short _port;
#if defined(_WIN32) && !defined(__CYGWIN__)
    SOCKADDR_IN saddr;
#else
    struct sockaddr_in saddr;
#endif
    unsigned long _address;
};
