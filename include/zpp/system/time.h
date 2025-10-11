#pragma once

#include <zpp/namespace.h>
#include <string>

#ifndef USE_BOOST
#include <chrono>
using namespace std;
#else
#include <boost/chrono.hpp>
#include <boost/chrono/system_clocks.hpp>
using namespace boost;
#endif
#include <ctime>

NSB_ZPP
class time : public std::tm{
public:
    chrono::system_clock::time_point tp;
    time_t tm_us;

    static time_t epoch_ms(){
        chrono::system_clock::duration duration_since_epoch = chrono::system_clock::now().time_since_epoch();
        return chrono::duration_cast<chrono::milliseconds>(duration_since_epoch).count();
    }
public:
    time()
        :tp(chrono::system_clock::now())
        ,tm_us(0){}
    time(time_t time_point)
        :tp(std::chrono::milliseconds(time_point))
        ,tm_us(0){}
    ~time() = default;

    void localtime(){
        chrono::system_clock::duration duration_since_epoch
            = tp.time_since_epoch();
        tm_us = chrono::duration_cast<chrono::microseconds>
            (duration_since_epoch).count();
        time_t seconds_since_epoch = tm_us / 1000000;

#if defined ZSYS_WINDOWS
        localtime_s(this, &seconds_since_epoch);
#elif defined __GNUC__
        localtime_r(&seconds_since_epoch, this);
#else
        this(*std::localtime(&seconds_since_epoch));
#endif

        //tm_xs %= 1000000;
        tm_year += 1900;
        tm_mon += 1;
    }

    void update(){
        tp = chrono::system_clock::now();
    }

     void seek(int sec){
        tp += chrono::seconds(sec);
    }

    template<class Dur=chrono::milliseconds>
    time_t time_span(){
        return std::chrono::duration_cast<Dur>(
            std::chrono::system_clock::now() - tp)
            .count();
    }

    time_t elapsed_ns(){
        return chrono::duration_cast<chrono::nanoseconds>
            (chrono::system_clock::now() - tp).count();
    }
    time_t elapsed_us(){
        return chrono::duration_cast<chrono::microseconds>
            (chrono::system_clock::now() - tp).count();
    }
    time_t elapsed_ms(){
        return chrono::duration_cast<chrono::milliseconds>
            (chrono::system_clock::now() - tp).count();
    }
    time_t elapsed_sec(){
        return chrono::duration_cast<chrono::seconds>
            (chrono::system_clock::now() - tp).count();
    }


    size_t printf(char *buf, size_t len){
#if defined ZSYS_WINDOWS
        return _snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d.%06lu",
                        tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec,
                        tm_us % 1000000);
#elif defined __GNUC__
        return snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d.%06lu",
                        tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec,
                        tm_us % 1000000);
#else
        return snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d.%06lu",
                        tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec,
                        tm_us % 1000000);
#endif

    }

    std::string& to_str(std::string& us){
        /* Efficiency Firstly
         * Not use std::sstream for save cpu resource.
         * Use parameter us to avoid 1 string construct/destruct.
         * yyyy-mm-dd hh:mm:ss.mmmuuu
         */
        us.resize(27);
        this->printf((char*)us.data(), 27);
        return us;
    }

protected:
};
NSE_ZPP
