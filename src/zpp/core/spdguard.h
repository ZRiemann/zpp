#pragma once

#include <vector>
#include <zpp/namespace.h>
#include <zpp/spdlog.h>

#include <spdlog/cfg/env.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"

NSB_ZPP

class spdguard{
public:
    spdguard(){
        sinks = new std::vector<std::shared_ptr<spdlog::sinks::sink>>;
    }
    spdguard &append_console_sink(){
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        set_sink_pattern(sink);
        sinks->push_back(sink);
        return *this;
    }
    spdguard &append_rotating_file(const spdlog::filename_t &base_filename = "spdlog_rotating.log", std::size_t max_size = 20, std::size_t max_files = 3, bool rotate_on_open = true){
        auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(base_filename, max_size * 1048576, max_files, rotate_on_open);
        set_sink_pattern(sink);
        sinks->push_back(sink);
        return *this;
    }
    spdguard &build_sync(){
        load_levels();
        g_spd_log = std::make_shared<spdlog::logger>("sync_loger", sinks->begin(), sinks->end());
        delete sinks;
#ifdef _DEBUG
        g_spd_log->info("sync spdlog(V{}.{}.{}) server you. Debug Version", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
#else
        g_spd_log->info("sync spdlog(V{}.{}.{}) server you. Release Version", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
#endif
        return *this;
    }
    spdguard &build_async(size_t queue_size = 8192){
        load_levels();
        spd_pool = std::make_shared<spdlog::details::thread_pool>(queue_size, 1);
        g_spd_log = std::make_shared<spdlog::async_logger>("async_logger", sinks->begin(), sinks->end(), spd_pool, spdlog::async_overflow_policy::block);
        spdlog::set_default_logger(g_spd_log);
        spdlog::flush_every(std::chrono::seconds(2));
        delete sinks;
#ifdef _DEBUG
        spd_inf("async spdlog(V{}.{}.{}) server you. Debug Version", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
#else
        spd_inf("async spdlog(V{}.{}.{}) server you. Release Version", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
#endif
        //g_spd_log->flush_on(spdlog::level::warn);
        return *this;
    }
    spdguard &build_level(spdlog::level::level_enum log_level = spdlog::level::trace){
        spdlog::set_level(log_level);
        return *this;
    }
    ~spdguard(){
        spd_inf("spdlog done.");
        g_spd_log->flush();
        spdlog::shutdown();
        g_spd_log.reset();
    }
    //std::shared_ptr<spdlog::logger> spd_log{nullptr};
    std::shared_ptr<spdlog::details::thread_pool> spd_pool{nullptr};
private:
    std::vector<std::shared_ptr<spdlog::sinks::sink>> *sinks;
    void load_levels(){}
    void set_sink_pattern(std::shared_ptr<spdlog::sinks::sink> sink){
        //sink->set_pattern("%d %H:%M:%S.%f %t %^%L%$ %v\t%s:%#:%!");
        sink->set_pattern("%d %H:%M:%S.%f %t %^%L%$ %v\t%g:%#");//:%!");
    }
};

NSE_ZPP