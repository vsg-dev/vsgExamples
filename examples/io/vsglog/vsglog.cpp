#include <vsg/all.h>

#include <iostream>

class CustomLogger : public vsg::Inherit<vsg::Logger, CustomLogger>
{
public:
    CustomLogger() {}

    void debug_implementation(const std::string& message) override
    {
        std::cout<<"custom debug : "<<message<<"\n";
    }

    void info_implementation(const std::string& message) override
    {
        std::cout<<"custom info : "<<message<<"\n";
    }

    void warn_implementation(const std::string& message) override
    {
        std::cerr<<"custom warn : "<<message<<std::endl;
    }

    void error_implementation(const std::string& message) override
    {
        std::cerr<<"custom error : "<<message<<std::endl;
    }
};


int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);

    auto count = arguments.value<size_t>(10, "-n");
    auto level = arguments.value(0, "-l");

    vsg::Logger::instance()->level = vsg::Logger::Level(level);

    vsg::info("info string");
    vsg::warn("warn string");
    vsg::error("error string");

    vsg::info("time ", 10, "ms, vector = (", vsg::vec3(10.0f, 20.0f, 30.0f), ")");
    //vsg::log()->info__stream()<<"second time "<<10<<"ms, vector = ("<<vsg::vec3(10.0f, 20.0f, 30.0f)<< ")"<<std::endl;

    vsg::Logger::instance()->info("third time ", 10, "ms, vector = (", vsg::vec3(10.0f, 20.0f, 30.0f), ")");
    vsg::info("forth time ", 30, "ms, vector = (", vsg::vec3(10.0f, 20.0f, 30.0f), ")");

    std::cout<<std::endl;

    // vsg::log() = CustomLogger::create();

    vsg::info("info string");
    vsg::warn("warn string");
    vsg::error("error string");
    vsg::error("here ", std::string("and"), " matrix  = {",vsg::dmat4{}, "}");

    std::cout<<std::endl;

    auto tick1 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::info("A line ", i);
    }

    auto tick3 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::Logger::instance()->info("D line ", i);
    }
    auto tick5 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::info("simple1");
    }
    auto tick6 = vsg::clock::now();

    vsg::Logger::instance() = CustomLogger::create();
    vsg::Logger::instance()->level = vsg::Logger::Level(level);

    auto tick7 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::info("simple2");
    }
    auto tick8 = vsg::clock::now();

    vsg::Logger::instance() = vsg::NullLogger::create();
    //vsg::log()->level = vsg::Logger::Level(level);

    auto tick11 = vsg::clock::now();
    for(size_t i=0; i<count; ++i)
    {
        vsg::info("simple4");
    }
    auto tick12 = vsg::clock::now();

    for(size_t i=0; i<count; ++i)
    {
        vsg::info("simple5", 10);
    }
    auto tick13 = vsg::clock::now();

    auto time1 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick3 - tick1).count();
    auto time4 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick5 - tick3).count();
    auto time5 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick6 - tick5).count();
    auto time6 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick8 - tick7).count();
    auto time8 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick12 - tick11).count();
    auto time9 = std::chrono::duration<double, std::chrono::milliseconds::period>(tick13 - tick12).count();

    vsg::Logger::instance() = vsg::StdLogger::create();
    vsg::Logger::instance()->level = vsg::Logger::LOGGER_ALL;

    vsg::info("info() time = ",time1,"ms");
    vsg::info("log->info() time = ",time4,"ms");
    vsg::info("vsg::info(\"simple\" time = ",time5,"ms");
    vsg::info("custom vsg::info(\"simple\" time = ",time6,"ms");
    vsg::info("null vsg::info(\"simple\" time = ",time8,"ms");
    vsg::info("null vsg::info(\"simple5\", 10) time = ",time9,"ms");

    return 0;
}
