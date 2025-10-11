#pragma once

#define Z_OS_LINUX 0
#define Z_OS_DRAGONFLY 0
#define Z_OS_FREEBSD 0
#define Z_OS_NETBSD 0
#define Z_OS_OPENBSD 0
#define Z_OS_DARWIN 0
#define Z_OS_WINDOWS 0
#define Z_OS_CNK 0
#define Z_OS_HURD 0
#define Z_OS_SOLARIS 0
#define Z_OS_UNIX 0

#ifdef _WIN32
#undef Z_OS_WINDOWS
#define Z_OS_WINDOWS 1
#endif

#ifdef __CYGWIN__
#undef Z_OS_WINDOWS
#define Z_OS_WINDOWS 1
#endif

#if (defined __APPLE__ && defined __MACH__)
#undef Z_OS_DARWIN
#define Z_OS_DARWIN 1
#endif

// in some ppc64 linux installations, only the second condition is met
#if (defined __linux)
#undef Z_OS_LINUX
#define Z_OS_LINUX 1
#elif (defined __linux__)
#undef Z_OS_LINUX
#define Z_OS_LINUX 1
#else
#endif

#if (defined __DragonFly__)
#undef Z_OS_DRAGONFLY
#define Z_OS_DRAGONFLY 1
#endif

#if (defined __FreeBSD__)
#undef Z_OS_FREEBSD
#define Z_OS_FREEBSD 1
#endif

#if (defined __NetBSD__)
#undef Z_OS_NETBSD
#define Z_OS_NETBSD 1
#endif

#if (defined __OpenBSD__)
#undef Z_OS_OPENBSD
#define Z_OS_OPENBSD 1
#endif

#if (defined __bgq__)
#undef Z_OS_CNK
#define Z_OS_CNK 1
#endif

#if (defined __GNU__)
#undef Z_OS_HURD
#define Z_OS_HURD 1
#endif

#if (defined __sun)
#undef Z_OS_SOLARIS
#define Z_OS_SOLARIS 1
#endif

#if (1 !=                                                                  \
     Z_OS_LINUX + Z_OS_DRAGONFLY + Z_OS_FREEBSD + Z_OS_NETBSD +        \
     Z_OS_OPENBSD + Z_OS_DARWIN + Z_OS_WINDOWS + Z_OS_HURD +           \
     Z_OS_SOLARIS)
#define Z_OS_UNKNOWN 1
#endif

#if Z_OS_LINUX || Z_OS_DRAGONFLY || Z_OS_FREEBSD || Z_OS_NETBSD ||     \
    Z_OS_OPENBSD || Z_OS_DARWIN || Z_OS_HURD || Z_OS_SOLARIS
#undef Z_OS_UNIX
#define Z_OS_UNIX 1
#endif

#if 0
#include <iostream>

#if defined(__APPLE__)
  #include <TargetConditionals.h>
#endif

int main() {
#if defined(_WIN32)
    #if defined(_WIN64)
      std::cout << "Windows 64-bit\n";
    #else
      std::cout << "Windows 32-bit\n";
    #endif
#elif defined(__APPLE__) && defined(__MACH__)
    #if defined(TARGET_OS_OSX) && TARGET_OS_OSX
      std::cout << "macOS\n";
    #elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
      std::cout << "iOS/iPadOS\n";
    #elif defined(TARGET_OS_TV) && TARGET_OS_TV
      std::cout << "tvOS\n";
    #elif defined(TARGET_OS_WATCH) && TARGET_OS_WATCH
      std::cout << "watchOS\n";
    #else
      std::cout << "Apple Darwin (unspecified)\n";
    #endif
#elif defined(__ANDROID__)
    std::cout << "Android (Linux-based)\n";
#elif defined(__linux__)
    std::cout << "Linux\n";
#elif defined(__unix__)
    std::cout << "UNIX-like (non-Linux)\n";
#elif defined(__CYGWIN__)
    std::cout << "Cygwin (POSIX layer on Windows)\n";
#elif defined(__MINGW64__) || defined(__MINGW32__)
    std::cout << "MinGW (Windows toolchain)\n";
#elif defined(__FreeBSD__)
    std::cout << "FreeBSD\n";
#elif defined(__NetBSD__)
    std::cout << "NetBSD\n";
#elif defined(__OpenBSD__)
    std::cout << "OpenBSD\n";
#elif defined(__sun) && defined(__SVR4)
    std::cout << "Solaris\n";
#else
    std::cout << "Unknown OS\n";
#endif
    return 0;
}
#endif