#include <zpp/error.h>
#include <zpp/spdlog.h>
#include <zpp/hpx/server.hpp>
#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/pipeline.hpp>
#include <zpp/taskflow/observer_worker_status.hpp>
#include <zpp/taskflow/worker_affine.hpp>
#include <zpp/system/system.h>
#include <zpp/system/sleep.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <hpx/hpx.hpp> // For hpx::get_os_thread_count
#include <algorithm> // For std::max

NSB_APP

USE_ZPP
class server : public z::zhpx::server{
public:
    server(int argc, char** argv);
    ~server() override;
    err_t run() override;
    err_t stop() override;
public:
    void example_pipeline_async();
private:
    tf::Executor _executor;
    std::shared_ptr<z::ztf::observer_worker_status> _observer;
};

server::server(int argc, char** argv)
    :z::zhpx::server(argc, argv)
    ,_executor(std::max(1, (int)z::sys::physical_cores() - (int)hpx::get_os_thread_count()), tf::make_worker_interface<z::ztf::affine_worker>())
    ,_observer{_executor.make_observer<z::ztf::observer_worker_status>()}{

    spd_inf("physical_cores: {}", z::sys::physical_cores());
    spd_inf("hpx_cores: {}", hpx::get_os_thread_count());
    spd_inf("executor num_workers: {}", _executor.num_workers());
}
server::~server(){
}

void server::example_pipeline_async(){
    spd_inf("================================================================================");
    spd_inf("example_pipeline_async (external feed)...");

    const size_t num_lines = 4;
    
    // Simulate an external data queue
    // Note: In a real app, these would be members of the class or passed in contexts
    std::queue<int> data_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::atomic<bool> stop_signal{false};

    // Buffer to hold data processing in the pipeline
    // Indexed by line number since each line handles one item at a time
    std::vector<int> line_data(num_lines, 0);

    // Producer thread simulating external async data source
    std::thread producer([&](){
        for(int i = 1; i <= 20; ++i) {
            z::sleep_ms(50); // simulate production interval
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                data_queue.push(i);
            }
            queue_cv.notify_one();
            spd_inf("Async Producer: pushed {}", i);
        }
        // Signal done
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            stop_signal = true;
        }
        queue_cv.notify_all();
    });

    tf::Taskflow taskflow("AsyncPipeline");
    tf::Pipeline pipeline(num_lines,
        // Stage 1: Fetch data (Serial) - waits for data
        tf::Pipe{tf::PipeType::SERIAL, [&](tf::Pipeflow& pf) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [&](){ return !data_queue.empty() || stop_signal; });

            if(data_queue.empty() && stop_signal){
                pf.stop();
                return;
            }

            // Pop data
            int val = data_queue.front();
            data_queue.pop();
            
            // Store into line buffer
            line_data[pf.line()] = val;
            
            spd_inf("Pipe1 (Fetch): Token {} / Line {} got data {}", pf.token(), pf.line(), val);
        }},

        // Stage 2: Process (Parallel)
        tf::Pipe{tf::PipeType::PARALLEL, [&](tf::Pipeflow& pf) {
            int val = line_data[pf.line()];
            // Simulate work
            z::sleep_ms(10 + (val % 5) * 10); 
            spd_inf("Pipe2 (Process): Token {} / Line {} processed {}", pf.token(), pf.line(), val);
        }},

        // Stage 3: Sink (Serial)
        tf::Pipe{tf::PipeType::SERIAL, [&](tf::Pipeflow& pf) {
             int val = line_data[pf.line()];
             spd_inf("Pipe3 (Sink): Token {} / Line {} finished {}", pf.token(), pf.line(), val);
        }}
    );

    taskflow.composed_of(pipeline).name("pipeline");
    
    _executor.run(taskflow).wait();

    if(producer.joinable()){
        producer.join();
    }
    spd_inf("async pipeline done.");
}

err_t server::run(){
    spd_inf("zpp_taskflow running...");
    spd_inf("begin sleep 100ms, wait workers to start...");
    sleep_ms(100);
    spd_inf("end sleep 100ms");
    example_pipeline_async();
    spd_inf("run done.");
    return ERR_OK;
}
err_t server::stop(){
    spd_inf("zpp_taskflow stopping...");
    return z::server::stop();
}

NSE_APP

#define SVR_NAME app::server
#include <zpp/hpx/main.hpp>