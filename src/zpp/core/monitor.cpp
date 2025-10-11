#include <zpp/core/monitor.h>
#include <zpp/spdlog.h>

z::mon_t z::monitor_guard::mont;

void z::monitor_guard::print_statistic(){
    mont.begin.localtime();
    std::string daytime;
    spd_inf("system start: {} time span: {} sec", mont.begin.to_str(daytime), mont.begin.elapsed_sec());
    
    uint64_t tasks{0}, task_cycles{0}, idel_cycles{0};
    for(int i = 0; i < MAX_THRS; ++i){
        thread_state state = mont.states[i];
        if(state.tasks == 0){
            continue;
        }

        spd_inf("thr[{}] {}:{}(ns) handle_tasks:{} tasks_elpased:{}(ns) idel_elpased:{}(ns) Q/S:{}", 
            i, state.state == 0 ? "IDEL" : "BUSY", CTS(z::elapsed_ns(state.start, z::tsc_now_r())),
            CTS(state.tasks), CTS(z::tsc_to_ns(state.task_cycles)), 
            CTS(z::tsc_to_ns(state.idel_cycles)), 
            CTS(state.tasks * 1000000000 / (z::tsc_to_ns(state.task_cycles + state.idel_cycles) + 1)));

        tasks += state.tasks;
        task_cycles += state.task_cycles;
        idel_cycles += state.idel_cycles;
    }

    int avg_task_cycles = tasks ? task_cycles / tasks : 0;
    int avg_idel_cycles = tasks ? idel_cycles / tasks : 0;
    spd_inf("total tasks:{} tasks_elpased:{}ns [avg:{} ns/q] idel_elpased:{}ns [avg:{} ns/q] Q/S[{}]",
        CTS(tasks), CTS(z::tsc_to_ns(task_cycles)), 
        CTS(z::tsc_to_ns(avg_task_cycles)), CTS(z::tsc_to_ns(idel_cycles)), CTS(z::tsc_to_ns(avg_idel_cycles)), 
        CTS(tasks * 1000000000 / (z::tsc_to_ns(task_cycles + idel_cycles) + 1)));
}
