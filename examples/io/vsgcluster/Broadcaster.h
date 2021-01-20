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
	#include <winsock.h>
#else
    #include <netinet/in.h>
#endif

std::vector<std::string> listNetworkConnections();

class Broadcaster : public vsg::Inherit<vsg::Object, Broadcaster>
{
public:

    Broadcaster();

    // Set the broadcast port
    void setPort( const short port );

    // Set the buffer to be broadcast
    void setBuffer( const void *buffer, unsigned int buffer_size );

    // Set the IFRName i.e. eth0, will default to platform appropriate setting where possible.
    void setIFRName( const std::string& name );


    // Set a recipient host.  If this is used, the Broadcaster
    // no longer broadcasts, but rather directs UDP packets at
    // host.
    void setHost( const char *hostname );

    // Sync broadcasts the buffer
    void sync( void );

    private :
    bool init( void );

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
    const void *_buffer;
    unsigned int _buffer_size;
#if defined(_WIN32) && !defined(__CYGWIN__)
    SOCKADDR_IN saddr;
#else
    struct sockaddr_in saddr;
#endif
    unsigned long _address;
};
