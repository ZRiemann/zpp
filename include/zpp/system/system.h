#pragma once

#include <zpp/namespace.h>
#include <zpp/system/os.h>

#include <string>

#if (Z_OS_WINDOWS) || (Z_OS_UNIX)
NSB_ZPP
class sys{
public:
    sys();
    ~sys();
    /**
     * @brief 显示操作系统相关信息
     */
    static void info();
    /**
     * @brief 获取当前进程的路经和名称
     */
    static void process_path(std::string &path, std::string &name);

    static std::size_t physical_cores();
};
NSE_ZPP
#else
#error not support
#endif