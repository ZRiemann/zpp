#pragma once

#ifndef USE_BOOST
#include <thread>
using namespace std;
#else
#include <boost/thread/v2/thread.hpp>
using namespace boost;
#endif

#include <zpp/namespace.h>

NSB_ZPP

inline void sleep_ms(int ms){
    this_thread::sleep_for(chrono::milliseconds(ms));
}

inline void sleep_us(time_t us){
    this_thread::sleep_for(chrono::microseconds(us));
}
NSE_ZPP
