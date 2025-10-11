#include "tests.h"

#include <zpp/spdlog.h>
#include <zpp/system/system.h>

void z::test_system(){
    z::sys::info();

    std::string path, name;
    z::sys::process_path(path, name);
    spd_inf("path[{}], name[{}]", path, name);
}