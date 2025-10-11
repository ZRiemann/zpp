#include <zpp/core/server.h>
#include <zpp/error.h>
#include <zpp/spdlog.h>
#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/pipeline.hpp>
#include <zpp/taskflow/observer_worker_status.hpp>
#include <zpp/taskflow/worker_affine.hpp>
#include <zpp/taskflow/pipeline_driver.hpp>
#include <zpp/taskflow/pipeline_mpsc.hpp>
#include <zpp/system/system.h>
#include <zpp/system/sleep.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <moodycamel/concurrentqueue.h>

NSB_APP

USE_ZPP
class server : public z::server{
public:
    server(int argc, char** argv);
    ~server() override;
    err_t run() override;
    err_t stop() override;
public:
    void example_pipeline();
    void example_pipeline_async(); // Deprecated: use example_pipeline_event_driven
    void example_pipeline_event_driven();
    void example_pipeline_driver();
    void example_pipeline_mpsc();
private:
    tf::Executor _executor;
    std::shared_ptr<z::ztf::observer_worker_status> _observer;
};

server::server(int argc, char** argv)
    :z::server(argc, argv)
    ,_executor(z::sys::physical_cores(), tf::make_worker_interface<z::ztf::affine_worker>())
    ,_observer{_executor.make_observer<z::ztf::observer_worker_status>()}{
    spd_inf("physical_cores: {}", z::sys::physical_cores());
    spd_inf("executor num_workers: {}", _executor.num_workers());
}
server::~server(){
}

void server::example_pipeline(){
    tf::Taskflow taskflow;

    const size_t num_lines = 4;
    const size_t num_pipes = 3;

    // create a custom data buffer
    std::array<std::array<int, num_pipes>, num_lines> buffer;

    // create a pipeline graph of four concurrent lines and three serial pipes
    tf::Pipeline pipeline(num_lines,
    // first pipe must define a serial direction
    tf::Pipe{tf::PipeType::SERIAL, [&buffer](tf::Pipeflow& pf) {
        // generate only 5 scheduling tokens
        if(pf.token() == 5) {
        pf.stop();
        }
        // save the token id into the buffer
        else {
        buffer[pf.line()][pf.pipe()] = pf.token();
        }
        spd_inf("token:{}, line:{}, pipe:{}, buf:{};", pf.token(), pf.line(), pf.pipe(), buffer[pf.line()][pf.pipe()]);
    }},
    tf::Pipe{tf::PipeType::PARALLEL, [&buffer] (tf::Pipeflow& pf) {
        // propagate the previous result to this pipe by adding one
        buffer[pf.line()][pf.pipe()] = buffer[pf.line()][pf.pipe()-1] + 1;
        spd_inf("token:{}, line:{}, pipe:{}, buf:{};", pf.token(), pf.line(), pf.pipe(), buffer[pf.line()][pf.pipe()]);
    }},
    tf::Pipe{tf::PipeType::SERIAL, [&buffer](tf::Pipeflow& pf){
        // propagate the previous result to this pipe by adding one
        buffer[pf.line()][pf.pipe()] = buffer[pf.line()][pf.pipe()-1] + 1;
        spd_inf("token:{}, line:{}, pipe:{}, buf:{};", pf.token(), pf.line(), pf.pipe(), buffer[pf.line()][pf.pipe()]);
    }}
    );

    // build the pipeline graph using composition
    tf::Task init = taskflow.emplace([](){ spd_inf("ready"); })
                            .name("starting pipeline");
    tf::Task task = taskflow.composed_of(pipeline)
                            .name("pipeline");
    tf::Task stop = taskflow.emplace([](){ spd_inf("stopped"); })
                            .name("pipeline stopped");

    // create task dependency
    init.precede(task);
    task.precede(stop);

    // run the pipeline
    _executor.run(taskflow).wait();
}
/**
 * @brief example_pipeline_async
 * @deprecated This method is obsolete due to its blocking nature. Use example_pipeline_event_driven instead.
 * @note An asynchronous pipeline that fetches data from an external source.
 * @warning This implementation uses a blocking wait (cv.wait) which occupies a worker thread throughout the lifecycle.
 *          Scalability Risk: If N concurrent pipelines run on M workers (where N >= M), all workers will be blocked waiting for data,
 *          causing Thread Pool Exhaustion and freezing the entire system.
 *          Use this ONLY when you can guarantee N_pipelines < N_workers or using dedicated threads.
 *          For high-concurrency systems, prefer `example_pipeline_event_driven()` (Reactive Pattern).
 */
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

/**
 * @brief example_pipeline_event_driven
 * @note An event-driven (reactive) pipeline that only runs when data is available.
 * @attention Advantages (Reactive Pattern):
 *            1. Zero Resource Waste: Workers are released immediately when queue is empty (pf.stop()).
 *            2. High Scalability: Supports M > N scenarios (M pipelines, N workers) without deadlock.
 *            3. No Busy Waiting: Unlike async version, no threads are blocked on cv.wait().
 *            Recommended for high-concurrency server applications sharing a global thread pool.
 */
void server::example_pipeline_event_driven(){
    spd_inf("================================================================================");
    spd_inf("example_pipeline_event_driven (reactive / non-blocking)...");

    const size_t num_lines = 4;

    // Shared context for the event-driven pipeline
    struct PipeContext {
        std::queue<int> queue;
        std::mutex mutex;
        bool is_running = false; // Is the pipeline currently scheduled/running?
        std::atomic<bool> producer_done {false};
        std::vector<int> buffer{std::vector<int>(4, 0)}; // Matches num_lines
    };
    auto ctx = std::make_shared<PipeContext>();

    tf::Taskflow taskflow("ReactivePipeline");
    
    // Define the pipeline
    // Key difference: The Source (Pipe 1) checks availability. 
    // If empty, it stops the specific pipeline run (token generation stops).
    // This allows the Taskflow to complete, freeing all workers.
    tf::Pipeline pipeline(num_lines,
        tf::Pipe{tf::PipeType::SERIAL, [ctx](tf::Pipeflow& pf){
            std::lock_guard<std::mutex> lock(ctx->mutex);
            if(ctx->queue.empty()){
                // Stop generating tokens for this batch.
                // The pipeline will spin down after existing tokens finish.
                spd_inf("Pipe1 (Stop) token:{}, line:{}, pipe:{}", pf.token(), pf.line(), pf.pipe());
                pf.stop(); 
            } else {
                int val = ctx->queue.front();
                ctx->queue.pop();
                ctx->buffer[pf.line()] = val;
                spd_inf("Pipe1 (Fetch): token:{}, line:{}, pipe:{}, val:{}", pf.token(), pf.line(), pf.pipe(), val);
            }
        }},
        tf::Pipe{tf::PipeType::PARALLEL, [ctx](tf::Pipeflow& pf){
            int val = ctx->buffer[pf.line()];
            z::sleep_ms(10 + (val%3)*10); // simulate work
            spd_inf("Pipe2 (Process): token:{}, line:{}, pipe:{}, val:{}", pf.token(), pf.line(), pf.pipe(), val);
        }},
        tf::Pipe{tf::PipeType::SERIAL, [ctx](tf::Pipeflow& pf){
             int val = ctx->buffer[pf.line()];
             spd_inf("Pipe3 (Sink): token:{}, line:{}, pipe:{}, val:{}", pf.token(), pf.line(), pf.pipe(), val);
        }}
    );
    taskflow.composed_of(pipeline);

    // Reschedule Logic: A recursive-style lambda to keep the pipeline running as long as data exists
    // We use std::function to allow recursive reference (captured by reference, but function object must exist)
    // Note: We need to be careful with lifetime of this function object.
    // Since we block the main thread at the end, local reference is safe.
    std::function<void()> run_pipeline_batch;
    
    run_pipeline_batch = [this, ctx, &taskflow, &run_pipeline_batch]() {
        // Critical Section: Check if we should run
        {
            std::lock_guard<std::mutex> lock(ctx->mutex);
            if(ctx->queue.empty()){
                ctx->is_running = false;
                return; // Nothing to do, go to sleep (release worker)
            }
            ctx->is_running = true;
        }

        // Submit the taskflow.
        // When it finishes (because Pipe1 stopped on empty queue), we verify if we need to restart.
        _executor.run(taskflow, [ctx, &run_pipeline_batch](){
            // This callback runs when the graph completes.
            // Check if more data arrived while we were running/stopping.
            run_pipeline_batch();
        });
    };

    // Producer Thread
    std::thread producer([ctx, &run_pipeline_batch](){
        for(int i = 1; i <= 20; ++i) {
            z::sleep_ms(20); // Produce slower than consume to test stopping/restarting
            bool trigger_needed = false;
            {
                std::lock_guard<std::mutex> lock(ctx->mutex);
                ctx->queue.push(i);
                spd_inf("Async Producer: pushed {}", i);
                
                // If pipeline is IDLE, we must wake it up.
                if(!ctx->is_running){
                    // Temporarily mark true to prevent double submission before run_pipeline_batch grabs lock
                    // But run_pipeline_batch re-checks logic. 
                    // Let's just trigger. The lock inside run_pipeline_batch handles concurrency.
                    // However, we shouldn't submit multiple concurrent taskflows for the same pipeline object 
                    // if it's not designed for it (Pipeline holds state).
                    // So we only trigger if !is_running.
                    if(!ctx->is_running) {
                         trigger_needed = true;
                         // Optimization: Set running=true here to block other producer pushes from triggering?
                         // But run_pipeline_batch sets it. 
                         // To be safe against race where run_pipeline_batch hasn't acquired lock yet:
                         ctx->is_running = true; 
                    }
                }
            }
            if(trigger_needed) run_pipeline_batch();
        }
        ctx->producer_done = true;
    });

    if(producer.joinable()) producer.join();

    // Wait for the pipeline to fully drain (consumer might still be working on last batch)
    while(true){
        {
            std::lock_guard<std::mutex> lock(ctx->mutex);
            if(!ctx->is_running && ctx->queue.empty()) break;
        }
        z::sleep_ms(10);
    }
    spd_inf("reactive pipeline done.");
}

void server::example_pipeline_driver(){
    spd_inf("================================================================================");
    spd_inf("example_pipeline_driver (encapsulated reactive)...");

    const size_t num_lines = 4;
    
    // Taskflow definitions
    tf::Taskflow taskflow("DriverPipeline");
    
    // 1. Create the driver wrapper
    // We use int as the data type.
    using Driver = z::ztf::pipeline_driver<int, moodycamel::ConcurrentQueue<int>>;
    //using Driver = z::ztf::pipeline_driver<int>;
    auto driver = std::make_shared<Driver>(_executor, taskflow, 1024);

    // 2. Data Buffer for the pipeline stages
    std::vector<int> buffer(num_lines, 0);

    // 3. Define Pipeline
    tf::Pipeline pipeline(num_lines,
        // Pipe 1: Source (SERIAL) - Auto-generated by driver
        driver->build_source([&buffer](tf::Pipeflow& pf, int&& val){
            buffer[pf.line()] = val;
            spd_inf("Pipe1 (Fetch): token:{}, line:{}, pipe:{}, val:{}", pf.token(), pf.line(), pf.pipe(), val);
        }),
        
        // Pipe 2: Process (PARALLEL)
        tf::Pipe{tf::PipeType::PARALLEL, [&buffer](tf::Pipeflow& pf){
            int val = buffer[pf.line()];
            z::sleep_ms(10 + (val%3)*10); // Simulate work
            spd_inf("Pipe2 (Process): token:{}, line:{}, pipe:{}, val:{}", pf.token(), pf.line(), pf.pipe(), val);
        }},
        
        // Pipe 3: Sink (SERIAL)
        tf::Pipe{tf::PipeType::SERIAL, [&buffer](tf::Pipeflow& pf){
            int val = buffer[pf.line()];
            spd_inf("Pipe3 (Sink): token:{}, line:{}, pipe:{}, val:{}", pf.token(), pf.line(), pf.pipe(), val);
        }}
    );

    taskflow.composed_of(pipeline);

    // 4. Producer Thread
    {
        std::jthread producer([driver](){
            for(int i = 1; i <= 20; ++i) {
                z::sleep_ms(20);
                
                // Push data into driver. 
                // Driver automatically schedules the Taskflow if it was idle.
                while(!driver->submit(i)) {
                    // 队列满时的自旋等待策略
                    z::sleep_ms(1); // 或者 std::this_thread::yield();
                    // spd_war("Pipeline congested, backing off...");
                }
                
                spd_inf("Driver Producer: pushed {}", i);
            }
        });
    }

    // 5. Wait for pipeline to drain
    driver->wait_done();
    spd_inf("driver pipeline done.");
}

void server::example_pipeline_mpsc(){
    spd_inf("================================================================================");
    spd_inf("example_pipeline_mpsc (specialized mpsc)...");

    const size_t num_lines = 4;
    
    // Taskflow definitions
    tf::Taskflow taskflow("MpscPipeline");
    
    // 1. Create the driver wrapper
    // We use int as the data type.
    using Driver = z::ztf::pipeline_mpsc<int>;
    auto driver = std::make_shared<Driver>(_executor, taskflow, 1024);

    // 2. Data Buffer for the pipeline stages
    std::vector<int> buffer(num_lines, 0);

    // 3. Define Pipeline
    tf::Pipeline pipeline(num_lines,
        // Pipe 1: Source (SERIAL) - Auto-generated by driver
        driver->build_source([&buffer](tf::Pipeflow& pf, int&& val){
            buffer[pf.line()] = val;
            spd_inf("Pipe1 (Fetch): token:{}, line:{}, pipe:{}, val:{}", pf.token(), pf.line(), pf.pipe(), val);
        }),
        
        // Pipe 2: Process (PARALLEL)
        tf::Pipe{tf::PipeType::PARALLEL, [&buffer](tf::Pipeflow& pf){
            int val = buffer[pf.line()];
            z::sleep_ms(10 + (val%3)*10); // Simulate work
            spd_inf("Pipe2 (Process): token:{}, line:{}, pipe:{}, val:{}", pf.token(), pf.line(), pf.pipe(), val);
        }},
        
        // Pipe 3: Sink (SERIAL)
        tf::Pipe{tf::PipeType::SERIAL, [&buffer](tf::Pipeflow& pf){
            int val = buffer[pf.line()];
            spd_inf("Pipe3 (Sink): token:{}, line:{}, pipe:{}, val:{}", pf.token(), pf.line(), pf.pipe(), val);
        }}
    );

    taskflow.composed_of(pipeline);

    // 4. Producer Thread
    {
        std::jthread producer([driver](){
            for(int i = 1; i <= 20; ++i) {
                z::sleep_ms(20);
                
                // Push data into driver. 
                // Driver automatically schedules the Taskflow if it was idle.
                while(!driver->submit(i)) {
                    std::this_thread::yield();
                }
                
                spd_inf("MPSC Producer: pushed {}", i);
            }
        });
    }

    // 5. Wait for pipeline to drain
    driver->wait_done();
    spd_inf("mpsc pipeline done.");
}

struct tf_data{
    int id{0};
};

static void base_example(tf::Executor& executor){
    tf::Taskflow taskflow("simple");
    tf_data data;

    tf::Task A = taskflow.emplace([&data](){
        spd_inf("TaskA"); 
    });

    A.name("Task_A");
    tf::Task B = taskflow.emplace([&data](){ 
        spd_inf("TaskB"); 
    });
    B.name("Task_B");
    tf::Task C = taskflow.emplace([&data](){ 
        spd_inf("TaskC"); 
    });
    C.name("Task_C");
    tf::Task D = taskflow.emplace([&data](){ 
        spd_inf("TaskD"); 
    });
    D.name("Task_D");

    A.precede(B, C);  // A runs before B and C
    D.succeed(B, C);  // D runs after  B and C

    executor.run(taskflow).wait();

    spd_inf("taskflow name:{}; num_tasks:{}", taskflow.name(), taskflow.num_tasks());
    std::string dot_str = taskflow.dump();
    spd_inf("taskflow dot:\n{}", dot_str);

    tf::Task parent = taskflow.emplace([&data](tf::Subflow& sf){
        auto F = sf.emplace([&data](){ 
            spd_inf("Subtask A"); 
        });
        F.name("Subtask_A");
        sf.retain(true);
    });
    parent.name("parent_task");
    parent.succeed(D);

    executor.run(taskflow).wait();
    spd_inf("After adding subflow:{}; num_tasks:{}", taskflow.dump(), taskflow.num_tasks());
    executor.run(taskflow).wait();
    /*
    测试结果： taskflow 的图对象在第一次 dump 后就固定下来了，
    后续即使添加了 subflow 任务，dump 出来的图对象也不会变化。
    需要在添加子任务后再次 dump 才能看到完整的图对象
    解决方法：在添加子任务后再次调用 dump 方法
    */
    spd_inf("Run again subflow:{}; num_tasks:{}", taskflow.dump(), taskflow.num_tasks());

    taskflow.for_each_task([](tf::Task t){
        spd_inf("Visiting task: {}", t.name());
    });

    /*
    只是解除了A和B的依赖关系，A仍然在C之前执行，D仍然在B和C之后执行
    B仍然会被执行，而不是不执行B
    需要重新设置依赖关系才能改变执行顺序
    例如，如果想让B在A之后执行，可以使用 B.succeed(A);
    这样B就会在A之后执行，而不受其他任务的影响
    也可以使用 A.precede(B); 来实现同样的效果
    */
    taskflow.remove_dependency(A, B);
    spd_inf("After removing dependency A->B: {}", taskflow.dump());
    tf::Future fu = executor.run(taskflow); // submit taskflow return future.
    fu.wait();
    //fu.get(); // get the result (blocking call)

    tf::Taskflow tf;
    auto a = tf.placeholder().name("a");
    auto b = tf.placeholder().name("b");
    auto c = tf.placeholder().name("c");
    auto d = tf.placeholder().name("d");

    a.precede(b, c, d);
    assert(a.num_successors() == 3);
    assert(b.num_predecessors() == 1);
    assert(c.num_predecessors() == 1);
    assert(d.num_predecessors() == 1);

    spd_inf("Placeholder taskflow before removal:\n{}", tf.dump());
    tf.remove_dependency(a, b);
    assert(a.num_successors() == 2);
    assert(b.num_predecessors() == 0);
    spd_inf("Placeholder taskflow:\n{}", tf.dump());
}

// Demonstrates a safe pattern for per-run runtime data: for each invocation
// create a small Taskflow that captures a `shared_ptr<tf_data>` by value.
// This ensures `tf_data` stays alive until the tasks complete. Collect the
// returned futures to observe completion and avoid data races.
static void data_driven_example(tf::Executor& executor){
    std::vector<decltype(executor.run(std::declval<tf::Taskflow&>()))> futs; // hold futures
    std::vector<std::shared_ptr<tf::Taskflow>> kept_taskflows; // keep taskflow objects alive
    for(int run_id = 0; run_id < 3; ++run_id){
        auto data = std::make_shared<tf_data>();

        auto taskflow = std::make_shared<tf::Taskflow>("per_run");

        // Capture `data` by value so tasks own a reference to it.
        taskflow->emplace([data,run_id](){
            
            spd_inf("Run {} - TaskA (uses data)", run_id);
            // use data->trk ... (example uses tracker API as needed)
        });

        taskflow->emplace([data,run_id](){
            
            spd_inf("Run {} - TaskB (uses data)", run_id);
        });

        // submit and keep future to ensure we can wait for completion later
        auto fut = executor.run(*taskflow);
        futs.emplace_back(std::move(fut));
        kept_taskflows.emplace_back(taskflow);
    }

    // wait for all runs to complete (ensures all `data` are still alive until done)
    for(auto &f : futs){
        f.wait();
    }
}

static void task_example(tf::Executor& executor){
    spd_inf("task example...");
    tf::Taskflow taskflow("task example");
    tf::Taskflow taskflow2("taskflow2");
    tf::Task task1 = taskflow.emplace([](){}).name("static task");
    tf::Task task2 = taskflow.emplace([](){ return 3; }).name("condition task");
    tf::Task task3 = taskflow.emplace([](tf::Runtime&){}).name("runtime task");
    tf::Task task4 = taskflow.emplace([](tf::Subflow& sf){
        tf::Task stask1 = sf.emplace([](){});
        tf::Task stask2 = sf.emplace([](){});
        stask1.precede(stask2);
    }).name("subflow task");

    taskflow2.emplace([](){}).name("tf2.static task");
    tf::Task task5 = taskflow.composed_of(taskflow2).name("module task");
    task1.precede(task2, task3, task4, task5);
    executor.run(taskflow).wait();
    spd_inf("task example{}", taskflow.dump());


    /*
     tf::Task is polymorphic. 
    Once created, you can assign a different task type to it using tf::Task::work.
    For example, the code below creates a static task and then reworks it to a subflow task:
    */
    task1.work([](tf::Subflow& sf){
        tf::Task stask1 = sf.emplace([](){});
        tf::Task stask2 = sf.emplace([](){});
        (void)stask1;
        (void)stask2;
    }).name("subflow task");

    // dependencies
    {
        tf::Taskflow taskflow("Dependencies");
        auto [init, cond, yes, no] = taskflow.emplace(
        [] () { },
        [] () { return 0; },
        [] () { std::cout << "yes\n"; },
        [] () { std::cout << "no\n"; }
        );
        cond.succeed(init)
            .precede(yes, no);  // executes yes if cond returns 0
                                // executes no  if cond returns 1
        spd_inf("Conditional tasking:\n{} num_strong_dependencies:{}, num_weak_dependencies:{}", taskflow.dump(), cond.num_strong_dependencies(), cond.num_weak_dependencies());
    }

    // assigns pointer to user data
    {
        tf::Taskflow taskflow("attach data to a task");
        
        int data;  // user data

        // create a task and attach it a user data
        auto A = taskflow.placeholder();
        A.data(&data).work([A](){
            auto d = *static_cast<int*>(A.data());
            spd_inf("data is {}", d);
        });

        // run the taskflow iteratively with changing data
        for(data = 0; data<3; data++){
            executor.run(taskflow).wait();
        }
    }

    // class test
    {
        class A{
        public:
            ~A() = default;
    
            A(int v):_value(v){}
            void print(){   
                spd_inf("Value is {}", _value);
            }
        private:
            int _value;
        };

        tf::Taskflow taskflow("class test");
        auto task = taskflow.placeholder();
        A a(42);
        A a1(100);
        task.data(&a).work([task](){
            auto a = static_cast<A*>(task.data());
            a->print();
        });
        executor.run(taskflow).wait();
        task.data(&a1);
        executor.run(taskflow).wait();
    }
    // reset task
    {
        tf::Taskflow taskflow("reset task");
        tf::Task task = taskflow.emplace([](){ spd_inf("task before reset"); });
        executor.run(taskflow).wait();
        //task.reset(); // reset() 只能让 task 变成 empty task，但是taskflow 中的节点还在，会被执行；
        task.reset_work(); // reset_work() 会把任务变成 placeholder，不会被执行。but not empty()
        spd_inf("After resetting task, is empty: {}", task.empty());
        executor.run(taskflow).wait(); // no task executed
    }

    // applies an visitor callable to each successor of the task
    {
        tf::Taskflow taskflow("for_each_successor");
        auto A = taskflow.emplace([](){ spd_inf("Task A"); }).name("Task_A");
        auto B = taskflow.emplace([](){ spd_inf("Task B"); }).name("Task_B");
        auto C = taskflow.emplace([](){ spd_inf("Task C"); }).name("Task_C");
        auto D = taskflow.emplace([](){ spd_inf("Task D"); }).name("Task_D");

        A.precede(B, C);
        D.succeed(B, C);

        spd_inf("Visiting successors of task A:");
        A.for_each_successor([](tf::Task t){
            spd_inf("Successor task: {}", t.name());
        });

        D.for_each_predecessor([](tf::Task pred){
            spd_inf("Predcessor task:{};", pred.name());
        });
        //executor.run(taskflow).wait();

        tf::Taskflow tf_1;
        tf::Task task = tf_1.emplace([](tf::Subflow& sf){
            tf::Task stask1 = sf.emplace([](){
                spd_inf("stask1");
            }).name("stask1");
            tf::Task stask2 = sf.emplace([](){
                spd_inf("stask2");
            }).name("stask2");
            (void)stask1;
            (void)stask2;
            sf.retain(true); // must retain to visit subflow tasks outside
        }).name("parent_task");
        spd_inf("Iterate tasks in the subflow and print each subflow task.");
        executor.run(tf_1).wait(); // need run to initialize subflow
        /*
        要想访问子任务，1）必须在子任务函数中调用 sf.retain(true); 这样子任务才会被保留，
        2）然后在运行 taskflow 之后，才能访问子任务
        */
        task.for_each_subflow_task([](tf::Task stask){
            spd_inf("subflow task {}", stask.name());
        });
    }
}

void example_runtime(tf::Executor& executor){
    spd_inf("runtime task example...");
    tf::Taskflow taskflow("Runtime Tasking");

    tf::Task A, B, C, D;

    std::tie(A, B, C, D) = taskflow.emplace(
    [] () { 
        spd_inf("A");
        return; // B,C,D 所有任务都执行
        // 返回不同的值来决定执行哪个任务
        //return 0; // B
        // return 1; // C
        // return 2; // D
    },
#if 1
    [&C] (tf::Runtime& rt) {  // C must be captured by reference
        spd_inf("B");
        rt.schedule(C);
        //(void)rt;
    },
#else
    [] () {
        spd_inf("B");
    },  
#endif
    [] () { spd_inf("C"); },
    [] () { spd_inf("D"); }
    );

    // name tasks
    A.name("A");
    B.name("B");
    C.name("C");
    D.name("D");

    // create conditional dependencies
    A.precede(B, C, D);

    // dump the graph structure
    spd_inf("taskflow dump:\n{}", taskflow.dump());

    // we will see both B and C in the output
    std::shared_ptr<tf::ChromeObserver> observer = executor.make_observer<tf::ChromeObserver>();
    executor.run(taskflow).wait();
    std::string str = observer->dump();
    executor.remove_observer(std::move(observer));
    spd_inf("Chrome Tracing dump:\n{}", str);
}

/**
 * @brief 静态mission设计思路
 * @note
 * 1. 流程对象池，避免taskflow的重复分配
 * 2. 采用替换data，执行不同的数据处理
 */
static void example_attach_data(tf::Executor& executor){
    spd_inf("================================================================================");
    spd_inf("example attach data to task...");
    tf::Taskflow taskflow("attach data to a task");
    
    int data;  // user data
#if 0
    // create a task and attach it the data
    auto A = taskflow.emplace([A](){
        auto d = *static_cast<int*>(A.data());
        spd_inf("data is {}", d);
    });
    A.data(&data);
#else
    auto A = taskflow.placeholder();
    A.data(&data).work([A](){
        auto d = *static_cast<int*>(A.data());
        spd_inf("data is {}", d);
    });
#endif
    // run the taskflow iteratively with changing data
    for(data = 0; data<3; data++){
        executor.run(taskflow).wait();
    }
}

static void example_async(tf::Executor& executor){
    spd_inf("================================================================================");
    spd_inf("example async tasks...");
    // create asynchronous tasks from the executor
    // (using executor as a thread pool)
    std::future<int> fu1 = executor.async([](){
      spd_inf("async task returns 1");
      return 1;
    });

    executor.silent_async([](){  // silent async task doesn't return any future object
      spd_inf("silent async does not return");
    });

    // create async tasks with runtime
    std::future<void> fu2 = executor.async([](tf::Runtime& rt){
      spd_inf("async task with a runtime: {}", (void*)&rt);
    });

    executor.silent_async([](tf::Runtime& rt){
      spd_inf("silent async task with a runtime: {}", (void*)&rt);
    });

    // 测试async lambda的可复用性
    executor.wait_for_all();  // wait for the two async tasks to finish
}

err_t server::run(){
    spd_inf("zpp_taskflow running...");
    spd_inf("begin sleep 100ms, wait workers to start...");
    sleep_ms(100);
    spd_inf("end sleep 100ms");
    base_example(_executor);
    data_driven_example(_executor);
    task_example(_executor);
    example_runtime(_executor);
    example_attach_data(_executor);
    example_async(_executor);
    example_pipeline();
    example_pipeline_async();
    example_pipeline_event_driven();
    example_pipeline_driver();
    example_pipeline_mpsc();
    spd_inf("run done.");
    return ERR_OK;
}

err_t server::stop(){
    spd_inf("zpp_taskflow stopping...");
    return z::server::stop();
}
NSE_APP

#define SVR_NAME app::server
#include <zpp/core/main.hpp>