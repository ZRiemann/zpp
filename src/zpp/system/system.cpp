#include <zpp/system/system.h>

#include <stdlib.h>
#include <cstring>

#include <zpp/namespace.h>
#include <zpp/spdlog.h>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#elif(defined __unix__)
#include <unistd.h>
#include <errno.h>
#include <sys/utsname.h>
#else
#error not support
#endif

USE_ZPP

sys::sys(){}
sys::~sys(){}

std::size_t sys::physical_cores(){
    return std::max(1u, std::thread::hardware_concurrency());
}

struct OSInfo {
    std::string name;       // OS 名称/内核名
    std::string version;    // 版本/内核版本
    std::string arch;       // 架构
    std::string hostname;   // 主机名
};

#if Z_OS_WINDOWS
typedef LONG (WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

void sys::info(){
    OSInfo info;

    // 主机名
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
        char hn[256] = {0};
        DWORD sz = sizeof(hn);
        if (GetComputerNameA(hn, &sz))
            info.hostname = hn;
        WSACleanup();
    }

    // 架构
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: info.arch = "x64"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: info.arch = "arm64"; break;
        case PROCESSOR_ARCHITECTURE_ARM:   info.arch = "arm"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: info.arch = "x86"; break;
        default: info.arch = "unknown"; break;
    }

    // 名称 + 版本（简化）
    info.name = "Windows";
    // 尝试用 RtlGetVersion 获取真实版本（不受清单/兼容性影响）
    HMODULE hNt = GetModuleHandleW(L"ntdll.dll");
    if (hNt) {
        auto rtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(hNt, "RtlGetVersion"));
        if (rtlGetVersion) {
            RTL_OSVERSIONINFOW rovi{};
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (rtlGetVersion(&rovi) == 0) {
                info.version = std::to_string(rovi.dwMajorVersion) + "." +
                               std::to_string(rovi.dwMinorVersion) + "." +
                               std::to_string(rovi.dwBuildNumber);
                spd_inf("system infomation:\nname: {}\nversion: {}\narch: {}\nhostname: {}\n", info.name, info.version, info.arch, info.hostname);
                return;
            }
        }
    }
    // 兜底：GetVersionEx 已弃用且可能返回错误版本，这里不再调用。作为占位：
    info.version = "unknown";
}
#elif Z_OS_UNIX
void sys::info(){
    OSInfo info;
    struct utsname u{};
    if (uname(&u) == 0) {
        info.name = u.sysname;    // Linux, Darwin, FreeBSD...
        info.version = std::string(u.release) + " (" + u.version + ")";
        info.arch = u.machine;    // x86_64, aarch64/arm64...
    } else {
        info.name = "Unknown Unix";
    }
    // 主机名
    char hn[256] = {0};
    if (gethostname(hn, sizeof(hn)) == 0) info.hostname = hn;
    spd_inf("system infomation:\nname: {}\nversion: {}\narch: {}\nhostname: {}\n", info.name, info.version, info.arch, info.hostname);
}
#endif
void sys::process_path(std::string &path, std::string &name){
    const int max_size = 256;
    if(path.capacity() < max_size){
        path.resize(max_size, 0);
    }
#ifdef _WIN32
    int len = GetModuleFileNameA(NULL,(char*)path.data(), max_size);
    if( 0 == len ){
        //return GetLastError();
        return;
    }
    size_t pos = path.rfind('\\');
#elif(defined __unix__)
    ssize_t len = readlink("/proc/self/exe", (char*)path.data(), max_size);
    if(-1 == len){
        //return errno;
        return;
    }
    size_t pos = path.rfind('/');
#else
    return;
#endif
    if(std::string::npos == pos){
        return;
    }
    name = path.substr(pos + 1, strlen(path.c_str()) - pos - 1);
    path = path.substr(0, pos + 1);
}