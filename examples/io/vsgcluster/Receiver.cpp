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

#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#if defined(_WIN32) && !defined(__CYGWIN__)
#    define NOMINMAX
#    include <winsock.h>
#else
#    include <arpa/inet.h>
#    include <errno.h>
#    include <netdb.h>
#    include <netinet/in.h>
#    include <sys/socket.h>
#    include <sys/time.h>
#    include <sys/uio.h>
#    include <unistd.h>
#endif
#include <string.h>

#include "Receiver.h"

#include <iostream>

Receiver::Receiver(uint16_t port) :
    _initialized(false),
    _port(port)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    WORD version = MAKEWORD(1, 1);
    WSADATA wsaData;
    // First, we start up Winsock
    WSAStartup(version, &wsaData);
#endif
}

Receiver::~Receiver(void)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    closesocket(_so);

    WSACleanup();
#else
    close(_so);
#endif
}

bool Receiver::init(void)
{
    if (_port == 0)
    {
        fprintf(stderr, "Receiver::init() - port not defined\n");
        return false;
    }

    if ((_so = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket");
        return false;
    }

#if defined(_WIN32) && !defined(__CYGWIN__)
    const BOOL on = TRUE;
    setsockopt(_so, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(int));
#else
    int on = 1;
    setsockopt(_so, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#endif

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(_port);
#if defined(_WIN32) && !defined(__CYGWIN__)
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
#else
    saddr.sin_addr.s_addr = 0;
#endif

    // set up a 1 second timeout.
#if defined(_WIN32) && !defined(__CYGWIN__)
    DWORD tv = 1000; // 1 sec in ms
    if (setsockopt(_so, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(DWORD)))
    {
        perror("setsockopt");
        return false;
    }
#else
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(_so, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)))
    {
        perror("setsockopt");
        return false;
    }
#endif

    if (bind(_so, (struct sockaddr*)&saddr, sizeof(saddr)) < 0)
    {
        perror("bind");
        return false;
    }

    _initialized = true;
    return _initialized;
}

unsigned int Receiver::recieve(void* buffer, const unsigned int buffer_size)
{
    if (!_initialized) init();

    if (buffer == 0L)
    {
        fprintf(stderr, "Receiver::sync() - No buffer\n");
        return 0;
    }

#if defined(__linux) || defined(__FreeBSD__) || defined(__APPLE__) || defined(__FreeBSD_kernel__) || defined(__GNU__)
    socklen_t size;
#else
    int size;
#endif
    size = sizeof(struct sockaddr_in);

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(_so, &fdset);

#if defined(_WIN32) && !defined(__CYGWIN__)

    int read_bytes = recvfrom(_so, (char*)buffer, buffer_size, 0, (sockaddr*)&saddr, &size);

    if (read_bytes < 0)
    {
        int err = WSAGetLastError();
        if (err == WSAETIMEDOUT)
        {
            std::cout << "Receiver::sync() : Connection timed out." << std::endl;
            return 0;
        }

        if (err != 0)
        {
            wchar_t* s = NULL;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, WSAGetLastError(),
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPWSTR)&s, 0, NULL);
            fprintf(stderr, "Receiver::sync() - error  : %S\n", s);
            LocalFree(s);
        }

        return 0;
    }

#else

    ssize_t read_bytes = recvfrom(_so, (caddr_t)buffer, buffer_size, 0, 0, &size);

    if (read_bytes < 0)
    {
        std::cerr << "Receiver::sync() : " << strerror(errno) << std::endl;
        return 0;
    }

#endif

    return static_cast<unsigned int>(read_bytes);
}
