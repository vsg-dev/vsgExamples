#include <vsg/all.h>

#include <iostream>

class ThreadLogger : public vsg::Inherit<vsg::Logger, ThreadLogger>
{
public:
    ThreadLogger() {}

    void setThreadPrefix(std::thread::id id, const std::string& str)
    {
        std::scoped_lock<std::mutex> lock(_mutex);
        _threadPrefixes[id] = str;
    }

    std::string debugPrefix = "debug: ";
    std::string infoPrefix = "info: ";
    std::string warnPrefix = "Warning: ";
    std::string errorPrefix = "ERROR: ";

protected:

    std::map<std::thread::id, std::string> _threadPrefixes;

    void print_id(std::ostream& out, std::thread::id id)
    {
        auto itr = _threadPrefixes.find(id);
        if (itr != _threadPrefixes.end())
        {
            out<<itr->second;
        }
        else
        {
            out<<"thread::id = "<<id<<": ";
        }
    }

    void debug_implementation(std::string_view message) override
    {
        print_id(std::cout, std::this_thread::get_id());
        std::cout<<debugPrefix<<message<<"\n";
    }

    void info_implementation(std::string_view message) override
    {
        print_id(std::cout, std::this_thread::get_id());
        std::cout<<infoPrefix<<message<<"\n";
    }

    void warn_implementation(std::string_view message) override
    {
        print_id(std::cout, std::this_thread::get_id());
        std::cerr<<warnPrefix<<message<<std::endl;
    }

    void error_implementation(std::string_view message) override
    {
        print_id(std::cout, std::this_thread::get_id());
        std::cerr<<errorPrefix<<message<<std::endl;
    }
};

struct MyOperation : public vsg::Inherit<vsg::Operation, MyOperation>
{
    uint32_t value = 0;
    MyOperation(uint32_t in_value) : value(in_value) {}

    void run() override
    {
        auto level = vsg::Logger::Level(1 + value % 4);
        vsg::info("info() operation ",value);
        vsg::log(level, "log() operation ",value);
    }
};

int main(int argc, char** argv)
{
    // assign our custom ThreadLogger
    auto mt_logger = ThreadLogger::create();
    vsg::Logger::instance() = mt_logger;

    // set thread main thread prefix
    mt_logger->setThreadPrefix(std::this_thread::get_id(), "main | ");

    vsg::CommandLine arguments(&argc, argv);
    auto numThreads = arguments.value<size_t>(16, "-t");
    auto count = arguments.value<size_t>(100, "-n");
    auto level = vsg::Logger::Level(arguments.value(0, "-l"));

    // default to logger level 0 to print all messages, but allow command line to override.
    vsg::Logger::instance()->level = level;

    auto operationThreads = vsg::OperationThreads::create(numThreads);
    uint32_t threadNum = 0;
    for(auto& thread : operationThreads->threads)
    {
        auto prefix = vsg::make_string("thread ", threadNum++, " | ");
        mt_logger->setThreadPrefix(thread.get_id(), prefix);
        vsg::info("set thread prefix for thread::id = ", thread.get_id(), " to ", prefix);
    }

    vsg::info("Adding ", count, " MyOperations.");

    // add operations to the OperationThreads queue.
    // note, as the threads are already running they can immediately pull these operations of the queue as we add them,
    // so by the end this loop many will already have been processed.
    for(size_t i=0; i<count; ++i)
    {
        operationThreads->add(MyOperation::create(i));
    }

    // have the main thread get and run operations from the OperationThreads queue of operations

    vsg::info("Starting to process operations.");
    while(auto op = operationThreads->queue->take())
    {
        op->run();
    }
    vsg::info("Finished processing operations.");

    // destroy the threads, some operations may still be running so these will complete before destruction is allowed to complete
    operationThreads = {};

    vsg::info("OperationThreads destroyed.");

    return 0;
}
