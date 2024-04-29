#include <vsg/all.h>

#include <iostream>

class CustomLogger : public vsg::Inherit<vsg::Logger, CustomLogger>
{
public:
    CustomLogger() {}

protected:
    void debug_implementation(const std::string_view& message) override
    {
        fprintf(stdout, "custom debug: %.*s\n", static_cast<int>(message.length()), message.data());
    }

    void info_implementation(const std::string_view& message) override
    {
        fprintf(stdout, "custom info: %.*s\n", static_cast<int>(message.length()), message.data());
    }

    void warn_implementation(const std::string_view& message) override
    {
        fprintf(stderr, "custom warn: %.*s\n", static_cast<int>(message.length()), message.data());
    }

    void error_implementation(const std::string_view& message) override
    {
        fprintf(stderr, "custom error: %.*s\n", static_cast<int>(message.length()), message.data());
    }

    void fatal_implementation(const std::string_view& message) override
    {
        fprintf(stderr, "custom error: %.*s\n", static_cast<int>(message.length()), message.data());
        throw vsg::Exception{std::string(message)};
    }
};

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);

    auto count = arguments.value<size_t>(0, "-n");
    auto level = vsg::Logger::Level(arguments.value(0, "-l"));

    // you can override the message verbosity by setting the minimum level that will be printed.
    vsg::Logger::instance()->level = level;

    // if we want to redirect std::cout and std::cerr to the vsg::Logger call vsg::Logger::redirect_stdout()
    if (arguments.read({"--redirect-std", "-r"})) vsg::Logger::instance()->redirect_std();

    // simplest form of messaging gets passed to the vsg::Logger::instance().
    std::cout << "cout cstring" << std::endl;
    std::cerr << "cerr cstring" << std::endl;
    vsg::debug("debug string");
    vsg::info("info cstring");
    vsg::warn("warn cstring");
    vsg::error("error cstring");

    // you can call the logger directly.
    vsg::Logger::instance()->info("info2 cstring");

    // you can pass multiple fields to the info, these will be converted, in a thread safe way, to a string form by using Logger's internal ostringstream
    vsg::info("time ", 10, "ms, vector = (", vsg::vec3(10.0f, 20.0f, 30.0f), ")");

    // if you have code that prints to a ostream, then this can be adapted to work with vsg::Logger by using the info_stream(printFunction), passing in a lambda function to
    // do the IO on stream that the Logger provides to the print function.
    vsg::info_stream([](auto& fout) { fout << "second time " << 10 << "ms, vector = (" << vsg::vec3(10.0f, 20.0f, 30.0f) << ")"; });

    std::cout << std::endl;

    // override the default Logger with our locally implemented Logger
    vsg::Logger::instance() = CustomLogger::create();
    vsg::Logger::instance()->level = level;

    vsg::debug("debug string");
    vsg::info("info string");
    vsg::warn("warn string");
    vsg::error("error string");
    vsg::info("here ", std::string("is a"), " matrix  = {", vsg::dmat4{}, "}");

    // as an alternative to calling vsg::debug/info/warn/error(..) you can call vsg::log(msg_level, ..)
    vsg::log(vsg::Logger::LOGGER_DEBUG, std::string("string log debug"));
    vsg::log(vsg::Logger::LOGGER_DEBUG, "cstring log debug");
    vsg::log(vsg::Logger::LOGGER_INFO, "log info");
    vsg::log(vsg::Logger::LOGGER_WARN, "log warn");
    vsg::log(vsg::Logger::LOGGER_ERROR, "log error");

    if (count > 0)
    {
        std::cout << std::endl;
        // let's do some performance tesing

        auto tick1 = vsg::clock::now();
        for (size_t i = 0; i < count; ++i)
        {
            vsg::info("simple");
        }

        auto tick2 = vsg::clock::now();
        for (size_t i = 0; i < count; ++i)
        {
            vsg::info("line number ", i);
        }
        auto tick3 = vsg::clock::now();

        vsg::Logger::instance() = vsg::NullLogger::create();

        auto tick4 = vsg::clock::now();
        for (size_t i = 0; i < count; ++i)
        {
            vsg::info("simple");
        }
        auto tick5 = vsg::clock::now();

        for (size_t i = 0; i < count; ++i)
        {
            vsg::info("line number", i);
        }
        auto tick6 = vsg::clock::now();

        auto time1 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick2 - tick1).count();
        auto time2 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick3 - tick2).count();
        auto time3 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick5 - tick4).count();
        auto time4 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick6 - tick5).count();

        // To view the results we'll assign the StdLogger
        vsg::Logger::instance() = vsg::StdLogger::create();
        vsg::Logger::instance()->level = vsg::Logger::LOGGER_ALL;

        vsg::info("vsg::info(\"simple\") time = ", time1, "ms");
        vsg::info("vsg::info(\"line number i\" time = ", time2, "ms");
        vsg::info("null vsg::info(\"simple\") time = ", time3, "ms");
        vsg::info("null vsg::info(\"line number i\" time = ", time4, "ms");
    }

    return 0;
}
