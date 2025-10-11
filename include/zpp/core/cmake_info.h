#pragma once

#include <zpp/namespace.h>
#include <zpp/spdlog.h>
NSB_ZPP
static inline void cmake_info(){
    spd_inf("\n\nNAME: {}\nGIT_COMMIT_HASH: {}\nGIT_BRANCH: {}\nBUILD_TIME: {}\nVERSION: {}\n", PROJECT_NAME, GIT_COMMIT_HASH, GIT_BRANCH, BUILD_TIME, VERSION);
}
NSE_ZPP