#include "tests.h"

#include <zpp/spdlog.h>
#include <zpp/system/tsc.h>
#include <zpp/system/sleep.h>

void z::test_timestamp_counter(){
    if(!z::tsc_init()){
        spd_err("TSC frequency initialization failed!");
        return;
    }
    spd_inf("TSC frequency detected: {} Hz {} ns/hz", CTS(get_tsc_frequency()), CTS(get_tsc_ns_per()));
    tsc_t tsc = z::tsc_now_r();
    duration_t ns_relaxed = z::tsc_to_ns(tsc);
    spd_inf("TSC relaxed value read: {}, {}ns", CTS(tsc), CTS(ns_relaxed));
    tsc = z::tsc_now_s();
    duration_t ns_serialized = z::tsc_to_ns(tsc);
    spd_inf("TSC serialized value read: {}, {}ns", CTS(tsc), CTS(ns_serialized));
    
    z::sleep_ms(10);
    
    tsc = z::tsc_now_r();
    duration_t ns_relaxed_end = z::tsc_to_ns(tsc);
    spd_inf("TSC relaxed value read: {}, {}ns", CTS(tsc), CTS(ns_relaxed_end));
    tsc = z::tsc_now_s();
    duration_t ns_serialized_end = z::tsc_to_ns(tsc);
    spd_inf("TSC serialized value read: {}, {}ns", CTS(tsc), CTS(ns_serialized_end));
    
    spd_inf("TSC relaxed elapsed: {} ns", CTS(ns_relaxed_end - ns_relaxed));
    spd_inf("TSC serialized elapsed: {} ns", CTS(ns_serialized_end - ns_serialized));


    // test step_ns_r()
    tsc = z::tsc_now_r();
    z::sleep_ms(10);
    duration_t step_ns = z::step_ns_r(tsc);
    spd_inf("TSC relaxed step_ns_r(): {} ns", CTS(step_ns));
    // should be very small 45,365ns
    spd_inf("TSC relaxed step_ns_r(): {} ns", CTS(z::step_ns_r(tsc)));
    step_ns = z::step_ns_r(tsc);
    step_ns = z::step_ns_r(tsc);
    // should be very small 8ns
    spd_inf("TSC relaxed step_ns_r(): {} ns", CTS(step_ns));
}