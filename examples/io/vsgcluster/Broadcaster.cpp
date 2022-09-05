/* OpenSceneGraph example, osgcluster.
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

#include "Broadcaster.h"

#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include <sys/types.h>

#if !defined(WIN32) || defined(__CYGWIN__)
#    include <arpa/inet.h>
#    include <errno.h>
#    include <net/if.h>
#    include <netdb.h>
#    include <netinet/in.h>
#    include <sys/ioctl.h>
#    include <sys/socket.h>
#    include <sys/time.h>
#    include <sys/uio.h>
#endif

#include <string.h>

#if defined(__linux)
#    include <linux/sockios.h>
#    include <unistd.h>
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#    include <sys/sockio.h>
#    include <unistd.h>
#elif defined(__sgi)
#    include <net/soioctl.h>
#    include <unistd.h>
#elif defined(__CYGWIN__)
#    include <unistd.h>
#elif defined(__GNU__)
#    include <unistd.h>
#elif defined(__sun)
#    include <sys/sockio.h>
#    include <unistd.h>
#elif defined(__APPLE__)
#    include <sys/sockio.h>
#    include <unistd.h>
#elif defined(WIN32)
#elif defined(__hpux)
#    include <unistd.h>
#else
#    error Teach me how to build on this system
#endif

#define _VERBOSE 1

std::vector<std::string> listNetworkConnections()
{
    std::vector<std::string> ifr_names;

#if defined(__linux)
    int socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socketfd == -1)
    {
        return {};
    }

    struct ifreq ifr;
    int result = 0;

    ifr.ifr_ifindex = 1;
    while ((result = ioctl(socketfd, SIOCGIFNAME, &ifr)) != -1)
    {
        ifr_names.push_back(ifr.ifr_name);
        ++ifr.ifr_ifindex;
    }

    result = close(socketfd);
#endif

    return ifr_names;
}

Broadcaster::Broadcaster(const std::string& hostname, uint16_t port, const std::string& ifrName) :
    _ifr_name(ifrName),
    _initialized(false),
    _port(port),
    _address(0)
{
    if (_ifr_name.empty())
    {
    #if defined(__linux) || defined(__CYGWIN__)
        _ifr_name = "eth0";
    #elif defined(__sun)
        _ifr_name = "hme0";
    #elif !defined(WIN32)
        _ifr_name = "ef0";
    #endif
    }

#if defined(WIN32) && !defined(__CYGWIN__)
    WORD version = MAKEWORD(1, 1);
    WSADATA wsaData;
    // First, we start up Winsock
    WSAStartup(version, &wsaData);
#endif

    if (!hostname.empty())
    {
        struct hostent* h;
        if ((h = gethostbyname(hostname.c_str())) == 0L)
        {
            std::cerr<<"Broadcaster::setHost() - Cannot resolve an address for "<<hostname<<std::endl;
            _address = 0;
        }
        else
            _address = *((unsigned long*)h->h_addr);
    }
}

Broadcaster::Broadcaster(uint16_t port, const std::string& ifrName) :
    Broadcaster({}, port, ifrName)
{
}

Broadcaster::~Broadcaster(void)
{
#if defined(WIN32) && !defined(__CYGWIN__)
    closesocket(_so);

    WSACleanup();
#else
    close(_so);
#endif
}

bool Broadcaster::init(void)
{
    if (_port == 0)
    {
        fprintf(stderr, "Broadcaster::init() - port not defined\n");
        return false;
    }

    if ((_so = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Broadcaster::init() - socket error");
        return false;
    }

#if defined(WIN32) && !defined(__CYGWIN__)
    const BOOL on = TRUE;
    setsockopt(_so, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(int));
#else
    int on = 1;
    setsockopt(_so, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#endif

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(_port);
    if (_address != 0)
    {
        saddr.sin_addr.s_addr = _address;
    }
    else
    {
#if defined(WIN32) && !defined(__CYGWIN__)
        setsockopt(_so, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof(int));
        saddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
#else
        setsockopt(_so, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));

        struct ifreq ifr;
        strcpy(ifr.ifr_name, _ifr_name.c_str());

        if ((ioctl(_so, SIOCGIFBRDADDR, &ifr)) < 0)
        {
            printf(" ifr.ifr_name = %s\n", ifr.ifr_name);
            perror("Broadcaster::init() - cannot get Broadcast Address");
            return false;
        }
        saddr.sin_addr.s_addr = (((sockaddr_in*)&ifr.ifr_broadaddr)->sin_addr.s_addr);
#endif
    }

#define _VERBOSE 1
#ifdef _VERBOSE
    unsigned char* ptr = (unsigned char*)&saddr.sin_addr.s_addr;
    printf("Broadcast address : %u.%u.%u.%u\n", ptr[0], ptr[1], ptr[2], ptr[3]);
#endif

    _initialized = true;
    return _initialized;
}

void Broadcaster::broadcast(const void* buffer, unsigned int buffer_size)
{
    if (!_initialized) init();

    if (buffer == 0L)
    {
        fprintf(stderr, "Broadcaster::sync() - No buffer\n");
        return;
    }


#if defined(WIN32) && !defined(__CYGWIN__)

    int flags = 0;
    unsigned int size = sizeof(SOCKADDR_IN);
    int result = sendto(_so, (const char*)buffer, buffer_size, flags , (struct sockaddr*)&saddr, size);
    if (result == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        if (err != 0)
        {
            wchar_t* s = NULL;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, WSAGetLastError(),
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPWSTR)&s, 0, NULL);
            fprintf(stderr, "Broadcaster::sync() - error  : %S\n", s);
            LocalFree(s);
        }
    }

#else

    int flags = MSG_DONTWAIT;
    unsigned int size = sizeof(struct sockaddr_in);
    ssize_t result = sendto(_so, (const void*)buffer, buffer_size, flags , (struct sockaddr*)&saddr, size);

    if (result < 0 && errno==EAGAIN)
    {
        //std::cout<<"reeat sendto()"<<std::endl;
        flags = 0;
        result = sendto(_so, (const void*)buffer, buffer_size, flags, (struct sockaddr*)&saddr, size);
    }

    if (result < 0)
    {
        std::cerr << "Broadcaster::sync() - errno = "<<errno<<", error : " << strerror(errno) << std::endl;
        return;
    }

#endif
}
