#include <vsg/all.h>

#include <mutex>

namespace vsg
{
    void info(const char* str)
    {
        std::cout<<"info : "<<str<<")"<<std::endl;
    }

    void warn(const char* str)
    {
        std::cout<<"warn("<<str<<")"<<std::endl;
    }

    void critical(const char* str)
    {
        std::cout<<"certical("<<str<<")"<<std::endl;
    }

    void error(const char* str)
    {
        std::cout<<"error("<<str<<")"<<std::endl;
    }

    void debug(const char* str)
    {
        std::cout<<"debug("<<str<<")"<< std::endl;
    }

}

int main(int /*argc*/, char** /*argv*/)
{

    vsg::info("info string");
    vsg::warn("warn string");
    vsg::critical("critical string");
    vsg::error("error string");
    vsg::debug("debug string");

    return 0;
}
