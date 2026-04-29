#pragma once

#include <vsg/io/DatabasePager.h>
#include <vsg/io/Logger.h>
#include <vsg/io/read.h>
#include <vsg/io/ReaderWriter.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/threading/atomics.h>
#include <vsg/utils/ComputeBounds.h>

class DatabasePagerAutoscale : public vsg::Inherit<vsg::DatabasePager, DatabasePagerAutoscale>
{
public:
    void start(uint32_t numReadThreads = 4) override
    {
        vsg::debug("DatabasePager::start(", numReadThreads, ")");

        //
        // set up read thread(s)
        //
        auto readThread = [](DatabasePagerAutoscale& databasePager, const std::string& threadName) {
            vsg::debug("Started DatabaseThread read thread");

            auto local_instrumentation = shareOrDuplicateForThreadSafety(databasePager.instrumentation);
            if (local_instrumentation) local_instrumentation->setThreadName(threadName);

            while (databasePager.status->active())
            {
                auto plod = databasePager._requestQueue->take_when_available(databasePager.frameCount.load());
                if (plod)
                {
                    CPU_INSTRUMENTATION_L1_NC(databasePager.instrumentation, "DatabasePager read", COLOR_PAGER);

                    uint64_t frameDelta = databasePager.frameCount - plod->frameHighResLastUsed.load();

                    if (frameDelta > 1 || !vsg::compare_exchange(plod->requestStatus, vsg::PagedLOD::ReadRequest, vsg::PagedLOD::Reading))
                    {
                        vsg::debug("Expire read request : databasePager.frameCount = ", databasePager.frameCount, ", plod->frameHighResLastUsed.load() = ", plod->frameHighResLastUsed.load());
                        databasePager.requestDiscarded(plod);
                        continue;
                    }

                    ++plod->loadAttempts;

                    vsg::ref_ptr<vsg::Node> subgraph = plod->pending;
                    vsg::ref_ptr<vsg::Object> read_object;

                    if (!subgraph)
                    {
                        // vsg::info("read ", subgraph, " from ", plod->filename, " frameDelta = ", frameDelta, ", databasePager.frameCount = ", databasePager.frameCount, ", plod->frameHighResLastUsed.load() = ", plod->frameHighResLastUsed.load(), ", numActiveRequests = ", databasePager.numActiveRequests.load());
                        read_object = vsg::read(plod->filename, plod->options);

                        // Add scale node to show each model with the same size
                        if (vsg::ref_ptr<vsg::Node> node = read_object.cast<vsg::Node>())
                        {
                            vsg::ComputeBounds computeBounds;
                            node->accept(computeBounds);

                            vsg::dvec3 center = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
                            double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;
                            double radius_inv = 1.0 / radius;
                            auto scale = vsg::MatrixTransform::create(vsg::scale(radius_inv, radius_inv, radius_inv) * vsg::translate(-center));
                            scale->addChild(node);
                            subgraph = scale;
                            plod->bound = vsg::dsphere(center * radius_inv, 1.0);
                        }
                    }
                    else
                    {
                        // vsg::info("already read ", subgraph, " from ", plod->filename, " frameDelta = ", frameDelta, ", databasePager.frameCount = ", databasePager.frameCount, ", plod->frameHighResLastUsed.load() = ", plod->frameHighResLastUsed.load(), ", numActiveRequests = ", databasePager.numActiveRequests.load());
                    }

                    if (subgraph && compare_exchange(plod->requestStatus, vsg::PagedLOD::Reading, vsg::PagedLOD::Compiling))
                    {
                        {
                            std::scoped_lock<std::mutex> lock(databasePager.pendingPagedLODMutex);
                            plod->pending = subgraph;
                        }

                        try
                        {
                            // compile plod
                            if (auto result = databasePager.compileManager->compile(subgraph))
                            {
                                plod->requestStatus.exchange(vsg::PagedLOD::MergeRequest);

                                // info("DatabaserPager::start() compiled ", subgraph, ", success after ", plod->loadAttempts.load(), " loadAttempts");

                                // move to the merge queue;
                                databasePager._toMergeQueue->add(plod, result);
                            }
                            else
                            {
                                debug("DatabaserPager::start() unable to compile subgraph, discarding request ", subgraph);
                                databasePager.requestDiscarded(plod);
                            }
                        }
                        catch (...)
                        {
                            debug("DatabaserPager::start() compile threw exception, discarding request ", subgraph);
                            databasePager.requestDiscarded(plod);
                        }
                    }
                    else
                    {
                        if (auto read_error = read_object.cast<vsg::ReadError>())
                            vsg::warn(read_error->message);
                        else
                            vsg::warn("Failed to read ", plod, " ", plod->filename);

                        databasePager.requestDiscarded(plod);
                    }
                }
                else
                {
                    // sleep for a frame.
                    std::this_thread::sleep_for(std::chrono::milliseconds(16 * 2));
                }
            }
            vsg::debug("Finished DatabaseThread read thread");
        };

        auto deleteThread = [](DatabasePagerAutoscale& databasePager, const std::string& threadName) {
            vsg::debug("Started DatabaseThread deletethread");

            auto local_instrumentation = shareOrDuplicateForThreadSafety(databasePager.instrumentation);
            if (local_instrumentation) local_instrumentation->setThreadName(threadName);

            while (databasePager.status->active())
            {
                databasePager.deleteQueue->wait_then_clear();
            }
            vsg::debug("Finished DatabaseThread delete thread");
        };

        for (uint32_t i = 0; i < numReadThreads; ++i)
        {
            threads.emplace_back(readThread, std::ref(*this), vsg::make_string("DatabasePager read thread ", i));
        }

        threads.emplace_back(deleteThread, std::ref(*this), "DatabasePager delete thread ");
    }
};
