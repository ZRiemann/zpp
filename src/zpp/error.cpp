#include <cstring>
#include <zpp/error.h>

#ifdef _WIN32
#include <windows.h>
#elif(defined __unix__)
#include <unistd.h>
#include <errno.h>
#else
#error not support
#endif

const char *z::str_err(z::err_t err){
    thread_local const char *strerr = "err-unknown";

    static const char *errs[] = {
        "[0]err-ok",
        "[1]err-fail",
        "[2]err-not-support",
        "[3]err-memory-insufficient",
        "[4]err-out-of-range",
        "[5]err-not-exit",
        "[6]err-exist",
        "[7]err-param-invalid",
        "[8]err-again",
        "[9]err-timeout",
        "[10]err-end"
    };

    if((z::ERR_MASK & err) == z::ERR_MASK){
        if((z::ERR_MASK ^ err) < z::ERR_END){
            strerr = errs[ERR_MASK ^ err];
        }else{
            strerr = "err-invalid-code";
        }
    }else if(ERR_OK == err){
        strerr = errs[0];
    }else{
#ifdef __unix__
        strerr = std::strerror(err);
#elif(defined _WIN32)
        thread_local char buf[256];
        HMODULE hModule = NULL;
        LPSTR MessageBuffer;
        DWORD dwBufferLength;
        DWORD dwFormatFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_IGNORE_INSERTS |
            FORMAT_MESSAGE_FROM_SYSTEM;
        if (code >= NERR_BASE && code <= MAX_NERR) {
            hModule = LoadLibraryEx(TEXT("netmsg.dll"), NULL,
                                    LOAD_LIBRARY_AS_DATAFILE);
            if (hModule != NULL)
            dwFormatFlags |= FORMAT_MESSAGE_FROM_HMODULE;
        }

        if (dwBufferLength = FormatMessageA(dwFormatFlags, hModule, err,
                                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                            (LPSTR)&MessageBuffer, 0, NULL)) {
            MessageBuffer[dwBufferLength - 2] = 0;
            _snprintf(buf, 256 - 1, MessageBuffer);
            LocalFree(MessageBuffer);
        }
        if (NULL != hModule) {
            FreeLibrary(hModule);
        }
        strerr = buf;  
#endif
    }
    return strerr;
}