#include <vsg/all.h>

#include <mutex>

namespace vsg
{
    class Logger : public Object
    {
    public:
        Logger() {}

        enum Level
        {
            ALL = 0,
            INFO,
            WARN,
            ERROR,
            CRITICAL,
            OFF
        };

        Level level = ALL;

        virtual void info(const char* message) = 0;
        virtual void info(const std::string& message) = 0;
        virtual void info_no_mutex(const std::string& message) = 0;
        virtual void warn(const std::string& message) = 0;
        virtual void error(const std::string& message) = 0;
        virtual void critical(const std::string& message) = 0;

        template<typename... Args>
        void info(Args&&... args)
        {
            if (level > INFO) return;

            std::scoped_lock<std::mutex> lock(mutex);
            stream.str({});
            stream.clear();
            (stream << ... << args);
            info_no_mutex(stream.str().c_str());
        }

        std::mutex mutex;
        std::ostringstream stream;
    };

    class StdLogger : public Inherit<Logger, StdLogger>
    {
    public:
        StdLogger() {}

        void info(const char* message) override
        {
            if (level > INFO) return;

            std::scoped_lock<std::mutex> lock(mutex);
#if 0
            std::cout<<message<<std::endl;
#else
            std::cout<<message;
            std::cout.put('\n');
#endif
        }

        void info(const std::string& message) override
        {
            if (level > INFO) return;

            std::scoped_lock<std::mutex> lock(mutex);
#if 0
            std::cout<<message<<std::endl;
#else
            std::cout<<message;
            std::cout.put('\n');
#endif
        }

        void info_no_mutex(const std::string& message) override
        {
            if (level > INFO) return;
#if 0
            std::cout<<message<<std::endl;
#else
            std::cout<<message;
            std::cout.put('\n');
#endif
        }

        void warn(const std::string& message) override
        {
            if (level > WARN) return;

            std::scoped_lock<std::mutex> lock(mutex);
            std::cout<<message<<std::endl;
        }

        void critical(const std::string& message) override
        {
            if (level > CRITICAL) return;

            std::scoped_lock<std::mutex> lock(mutex);
            std::cerr<<message<<std::endl;
        }

        void error(const std::string& message) override
        {
            if (level > ERROR) return;

            std::scoped_lock<std::mutex> lock(mutex);
            std::cerr<<message<<std::endl;
        }

    };

    static ref_ptr<Logger>& log()
    {
        static ref_ptr<Logger> s_logger = StdLogger::create();
        return s_logger;
    }


    template<typename... Args>
    void info(Args&&... args)
    {
        std::ostringstream stream;
        (stream << ... << args);
        log()->info(stream.str());
    }

    template<typename... Args>
    void info2(Args&&... args)
    {
        log()->info(args...);
    }

    template<typename... Args>
    void info3(Args&&...)
    {
    }

    void info(const char* str)
    {
        log()->info(str);
    }


    template<typename... Args>
    void warn(Args&&... args)
    {
        std::ostringstream stream;
        (stream << ... << args);
        log()->warn(stream.str());
    }

    void warn(const char* str)
    {
        log()->warn(str);
    }

    template<typename... Args>
    void critical(Args&&... args)
    {
        std::ostringstream stream;
        (stream << ... << args);
        log()->warn(stream.str());
    }

    void critical(const char* str)
    {
        log()->critical(str);
    }

    template<typename... Args>
    void error(Args&&... args)
    {
        std::ostringstream stream;
        (stream << ... << args);
        log()->warn(stream.str());
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

    void info(const char* message) override
    {
        if (level > INFO) return;
        std::scoped_lock<std::mutex> lock(mutex);
        std::cout<<message<<std::endl;
    }

    void info(const std::string& message) override
    {
        if (level > INFO) return;
        std::scoped_lock<std::mutex> lock(mutex);
        std::cout<<message<<std::endl;
    }

    void info_no_mutex(const std::string& message) override
    {
        if (level > INFO) return;
#if 0
        std::cout<<message<<std::endl;
#else
        std::cout<<message;
        std::cout.put('\n');
#endif
    }

    void warn(const std::string& message) override
    {
        if (level > ERROR) return;
        std::scoped_lock<std::mutex> lock(mutex);
        std::cout<<message<<std::endl;
    }

    void critical(const std::string& message) override
    {
        if (level > CRITICAL) return;
        std::scoped_lock<std::mutex> lock(mutex);
        std::cerr<<message<<std::endl;
    }

    void error(const std::string& message) override
    {
        if (level > ERROR) return;
        std::scoped_lock<std::mutex> lock(mutex);
        std::cerr<<message<<std::endl;
    }

};

class QuickLogger : public vsg::Inherit<vsg::Logger, QuickLogger>
{
public:
    QuickLogger() {}

    void info(const char* message) override
    {
        if (level > INFO) return;
        std::scoped_lock<std::mutex> lock(mutex);
        std::cout<<message<<"\n";
    }

    void info(const std::string& message) override
    {
        if (level > INFO) return;
        std::scoped_lock<std::mutex> lock(mutex);
        std::cout<<message<<"\n";
    }

    void info_no_mutex(const std::string& message) override
    {
        if (level > INFO) return;
#if 0
        std::cout<<message<<std::endl;
#else
        std::cout<<message;
        std::cout.put('\n');
#endif
    }

    void warn(const std::string& message) override
    {
        if (level > WARN) return;

        std::scoped_lock<std::mutex> lock(mutex);
        std::cout<<message<<std::endl;
    }

    void critical(const std::string& message) override
    {
        if (level > CRITICAL) return;

        std::scoped_lock<std::mutex> lock(mutex);
        std::cerr<<message<<std::endl;
    }

    void error(const std::string& message) override
    {
        if (level > ERROR) return;

        std::scoped_lock<std::mutex> lock(mutex);
        std::cerr<<message<<std::endl;
    }

};

class QuickerLogger : public vsg::Inherit<vsg::Logger, QuickerLogger>
{
public:
    QuickerLogger() {}

    void info(const char* message) override
    {
        if (level > INFO) return;
        std::cout<<message;
        std::cout.put('\n');
    }

    void info(const std::string& message) override
    {
        if (level > INFO) return;
#if 1
        std::cout<<message;
        std::cout.put('\n');
#else
        std::cout<<message<<'\n';
#endif
    }

    void info_no_mutex(const std::string& message) override
    {
        if (level > INFO) return;
#if 0
        std::cout<<message<<std::endl;
#else
        std::cout<<message;
        std::cout.put('\n');
#endif
    }

    void warn(const std::string& message) override
    {
        if (level > WARN) return;
        std::scoped_lock<std::mutex> lock(mutex);
        std::cout<<message<<std::endl;
    }

    void critical(const std::string& message) override
    {
        if (level > CRITICAL) return;
        std::scoped_lock<std::mutex> lock(mutex);
        std::cerr<<message<<std::endl;
    }

    void error(const std::string& message) override
    {
        if (level > ERROR) return;
        std::scoped_lock<std::mutex> lock(mutex);
        std::cerr<<message<<std::endl;
    }

};

class NullLogger : public vsg::Inherit<vsg::Logger, NullLogger>
{
public:
    NullLogger() {}

    void info(const char*) override
    {
    }

    void info(const std::string&) override
    {
    }

    void info_no_mutex(const std::string&) override
    {
    }

    void warn(const std::string&) override
    {
    }

    void critical(const std::string&) override
    {
    }

    void error(const std::string&) override
    {
    }
};

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);

    auto count = arguments.value<size_t>(1000, "-n");
    auto level = arguments.value(0, "-l");

    vsg::log()->level = vsg::Logger::Level(level);

    vsg::info("info string");
    vsg::warn("warn string");
    vsg::critical("critical string");
    vsg::error("error string");

    vsg::info("time ", 10, "ms, vector = (", vsg::vec3(10.0f, 20.0f, 30.0f), ")");
    //vsg::log()->info_stream()<<"second time "<<10<<"ms, vector = ("<<vsg::vec3(10.0f, 20.0f, 30.0f)<< ")"<<std::endl;

    vsg::log()->info("third time ", 10, "ms, vector = (", vsg::vec3(10.0f, 20.0f, 30.0f), ")");
    vsg::info2("forth time ", 30, "ms, vector = (", vsg::vec3(10.0f, 20.0f, 30.0f), ")");

    std::cout<<std::endl;

    // vsg::log() = CustomLogger::create();

    vsg::info("info string");
    vsg::warn("warn string");
    vsg::critical("critical string");
    vsg::error("error string");
    vsg::error("here ", std::string("and"), " matrix  = {",vsg::dmat4{}, "}");

    std::cout<<std::endl;

    auto tick1 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::info("A line ", i);
    }
    auto tick2 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::info2("B line ", i);
    }
    auto tick2_5 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::info3("C line ", i);
    }
    auto tick3 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::log()->info("D line ", i);
    }
    auto tick5 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::info("simple1");
    }
    auto tick6 = vsg::clock::now();

    vsg::log() = QuickLogger::create();
    vsg::log()->level = vsg::Logger::Level(level);

    auto tick7 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::info("simple2");
    }
    auto tick8 = vsg::clock::now();

    vsg::log() = QuickerLogger::create();
    vsg::log()->level = vsg::Logger::Level(level);

    auto tick9 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::info("simple3");
    }
    auto tick10 = vsg::clock::now();

    vsg::log() = NullLogger::create();
    vsg::log()->level = vsg::Logger::Level(level);

    auto tick11 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::info("simple4");
    }
    auto tick12 = vsg::clock::now();

    for(size_t i=0; i<count; ++i)
    {
        vsg::info2("simple5", 10);
    }
    auto tick13 = vsg::clock::now();

    auto time1 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick2 - tick1).count();
    auto time1_5 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick2_5 - tick2).count();
    auto time2 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick3 - tick2_5).count();
    auto time4 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick5 - tick3).count();
    auto time5 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick6 - tick5).count();
    auto time6 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick8 - tick7).count();
    auto time7 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick10 - tick9).count();
    auto time8 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick12 - tick11).count();
    auto time9 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick13 - tick12).count();

    vsg::log() = vsg::StdLogger::create();
    vsg::log()->level = vsg::Logger::ALL;

    vsg::info("info() time = ",time1,"ms");
    vsg::info("info2() time = ",time1_5,"ms");
    vsg::info("info3() time = ",time2,"ms");
    vsg::info("log->info() time = ",time4,"ms");
    vsg::info("vsg::info(\"simple\" time = ",time5,"ms");
    vsg::info("quick vsg::info(\"simple\" time = ",time6,"ms");
    vsg::info("quicker vsg::info(\"simple\" time = ",time7,"ms");
    vsg::info("null vsg::info(\"simple\" time = ",time8,"ms");
    vsg::info("null vsg::info(\"simple5\", 10) time = ",time9,"ms");

    return 0;
}
