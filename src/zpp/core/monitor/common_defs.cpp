#include <zpp/core/monitor/common_defs.h>
#include <zpp/spdlog.h>
#include <zpp/system/tsc.h>

void z::print_thr_stat(const thr_stat_t* states, size_t num_threads){
    uint64_t tasks{0}, task_cycles{0}, idel_cycles{0};

    spd_inf("id\ttasks\t\ttask:ns\t\tavg:ns\t\tidel:ns\t\tavg:ns\t\tstate");
    for(size_t i = 0; i < num_threads; ++i){
        const thr_stat_t& ts = states[i];
        if(ts.tasks == 0){
            continue;
        }
        spd_inf("{}\t{}\t\t{}\t\t{}\t\t{}\t\t{}\t\t{}",
            i, CTS(ts.tasks), CTS(z::tsc_to_ns(ts.task_cycles)),
            CTS(z::tsc_to_ns(ts.task_cycles / ts.tasks)), 
            CTS(z::tsc_to_ns(ts.idel_cycles)), 
            CTS(z::tsc_to_ns(ts.idel_cycles / ts.tasks)),
            ts.state);
        tasks += ts.tasks;
        task_cycles += ts.task_cycles;
        idel_cycles += ts.idel_cycles;
    }
    if(tasks == 0){
        spd_inf("no tasks handled.");
        return;
    }
    spd_inf("{}\t{}\t\t{}\t\t{}\t\t{}\t\t{}",
        "x",
        CTS(tasks), CTS(z::tsc_to_ns(task_cycles)), 
        CTS(tasks ? z::tsc_to_ns(task_cycles) / tasks : 0), 
        CTS(z::tsc_to_ns(idel_cycles)), 
        CTS(tasks ? z::tsc_to_ns(idel_cycles) / tasks : 0));
}