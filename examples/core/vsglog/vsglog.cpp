#include <vsg/all.h>

#include <mutex>

namespace vsg
{
    class Logger : public Object
    {
    public:
        Logger() {}

        virtual void info(const std::string& message) = 0;
        virtual void warn(const std::string& message) = 0;
        virtual void critical(const std::string& message) = 0;
        virtual void error(const std::string& message) = 0;
    };

    class StdLogger : public Inherit<Logger, StdLogger>
    {
    public:
        StdLogger() {}

        void info(const std::string& message) override
        {
            std::scoped_lock<std::mutex> lock(mutex);
            std::cout<<"cout: "<<message<<std::endl;
        }

        void warn(const std::string& message) override
        {
            std::scoped_lock<std::mutex> lock(mutex);
            std::cout<<"cout: "<<message<<std::endl;
        }

        void critical(const std::string& message) override
        {
            std::scoped_lock<std::mutex> lock(mutex);
            std::cerr<<"cerr: "<<message<<std::endl;
        }

        void error(const std::string& message) override
        {
            std::scoped_lock<std::mutex> lock(mutex);
            std::cerr<<"cerr: "<<message<<std::endl;
        }

        std::mutex mutex;
    };

    static ref_ptr<Logger>& log()
    {
        static ref_ptr<Logger> s_logger = StdLogger::create();
        return s_logger;
    }

    void info(const char* str)
    {
        log()->info(str);
    }

    void warn(const char* str)
    {
        log()->warn(str);
    }

    void critical(const char* str)
    {
        log()->critical(str);
    }

    void error(const char* str)
    {
        log()->error(str);
    }

}

class CustomLogger : public vsg::Inherit<vsg::Logger, CustomLogger>
{
public:
    CustomLogger() {}

    void info(const std::string& message) override
    {
        std::scoped_lock<std::mutex> lock(mutex);
        std::cout<<"custom cout: "<<message<<std::endl;
    }

    void warn(const std::string& message) override
    {
        std::scoped_lock<std::mutex> lock(mutex);
        std::cout<<"custom cout: "<<message<<std::endl;
    }

    void critical(const std::string& message) override
    {
        std::scoped_lock<std::mutex> lock(mutex);
        std::cerr<<"custom cerr: "<<message<<std::endl;
    }

    void error(const std::string& message) override
    {
        std::scoped_lock<std::mutex> lock(mutex);
        std::cerr<<"custom cerr: "<<message<<std::endl;
    }

    std::mutex mutex;
};


int main(int /*argc*/, char** /*argv*/)
{

    vsg::info("info string");
    vsg::warn("warn string");
    vsg::critical("critical string");
    vsg::error("error string");

    vsg::log() = CustomLogger::create();

    vsg::info("info string");
    vsg::warn("warn string");
    vsg::critical("critical string");
    vsg::error("error string");

    return 0;
}
