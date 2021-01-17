#ifndef UTIL_THREADPOOL_H
#define UTIL_THREADPOOL_H

#include "Thread.h"

#include <stdexcept>
#include <thread>
#include <list>
#include <atomic>
#include <future>

#include <blockingconcurrentqueue.h>

#include "io/Format.h"

namespace MUtils::Profiler {
extern void FinishThread();
}

namespace util {
    template<class T> class ThreadPool {
        public:
            /// Pushes a work request with a more substantive return type
            template<class F, class... Args>
            auto queueWorkItem(F&& f, Args&&... args) 
            -> std::future<typename std::invoke_result<F, Args...>::type> {
                // build a task from the function invocation
                using return_type = typename std::invoke_result<F, Args...>::type;
                auto task = std::make_shared< std::packaged_task<return_type()> >(
                    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
                );

                // get future
                std::future<return_type> fut = task->get_future();

                // insert to queue
                if(!this->acceptRequests) {
                    throw std::runtime_error("work queue not accepting requests");
                }
                this->workQueue.enqueue([task](){ (*task)(); });

                return fut;
            }

            /// number of pending work items
            const size_t numPending() const {
                return this->workQueue.size_approx();
            }
            /// number of worker threads
            const size_t getNumWorkers() const {
                return this->numWorkers;
            }

            /**
             * Initializes a thread pool that does not create any threads. You are responsible for
             * calling `startWorkers()` later.
             */
            ThreadPool(const std::string &_name) : name(_name) {

            }

            /**
             * Initializes a thread pool with the given number of default threads.
             */
            ThreadPool(const std::string &_name, const size_t numThreads) : name(_name) {
                this->numWorkers = numThreads;
                this->startWorkers(numThreads);
            }

            /**
             * Ensures the worker threads get terminated on dealloc.
             */
            virtual ~ThreadPool() {
                this->stopWorkers();
            }
        
            /**
             * Prepares the work queue deallocation by stopping acceptance of new work, and stopping existing workers.
             */
            virtual void cleanup() {
                this->stopWorkers(true);
            }

        protected:
            virtual void workerMain(size_t i) {
                Thread::setName(f("{} {}", this->name, i));
                this->workerThreadStarted(i);

                // main loop; dequeue work items
                T item;
                while(this->workerRun) {
                    this->workQueue.wait_dequeue(item);
                    item();
                }
                
                this->workerThreadWillEnd(i);
            } 

            /// Callback invoked when a thread is started
            virtual void workerThreadStarted(const size_t i) {};
            /// Callback invoked when a thread is exiting
            virtual void workerThreadWillEnd(const size_t i) {
                MUtils::Profiler::FinishThread();
            };
        
            /**
             * Starts `num` worker threads.
             */
            void startWorkers(size_t num) {
                // start workers
                this->workerRun = true;

                for(size_t i = 0; i < num; i++) {
                    auto worker = std::make_unique<std::thread>(&ThreadPool::workerMain, this, i);
                    this->workers.push_back(std::move(worker));
                }

                // once the workers are started, allow accepting requests
                this->acceptRequests = true;
                this->numWorkers = num;
            }

            /**
             * Shuts down all existing worker threads. Optionally, we wait for the work queue to
             * be drained as well.
             */
            void stopWorkers(bool drain = false) {
                // stop flag
                this->acceptRequests = false;
                this->workerRun = false;

                for(size_t i = 0; i < this->numWorkers+1; i++) {
                    this->workQueue.enqueue([&] {});
                }

                // wait for all threads to join
                for(auto &thread : this->workers) {
                    thread->join();
                }
                this->workers.clear();
            }

            /// number of worker threads to create
            size_t numWorkers = std::thread::hardware_concurrency() / 2;
            /// worker thread processes requests as long as this is set
            std::atomic_bool workerRun;
            /// worker threads
            std::list<std::unique_ptr<std::thread>> workers;
            /// work requests sent to the thread
            moodycamel::BlockingConcurrentQueue<T> workQueue;

            /// whether work is accepted
            bool acceptRequests;
        
            /// thread pool name (used for naming threads)
            std::string name = "unnamed thread pool";
    };
}

#endif
