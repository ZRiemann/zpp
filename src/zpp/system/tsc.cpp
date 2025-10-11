#include <zpp/system/tsc.h>



NSB_ZPP
double s_tsc_frequency{0.0};
double s_tsc_ns_per{0.0};

double detect_tsc_frequency_hz(){
    double freq = 0.0;
#if defined(_MSC_VER)
    int regs[4] = {0, 0, 0, 0};
    __cpuidex(regs, 0x15, 0);
    const unsigned int denom = static_cast<unsigned int>(regs[0]);
    const unsigned int numer = static_cast<unsigned int>(regs[1]);
    const unsigned int crystal = static_cast<unsigned int>(regs[2]);
    if (denom && numer && crystal){
        freq = (static_cast<double>(crystal) * numer) / denom;
    }
    if (freq == 0.0){
        __cpuidex(regs, 0x16, 0);
        const unsigned int mhz = static_cast<unsigned int>(regs[0]);
        if (mhz != 0U){
            freq = static_cast<double>(mhz) * 1'000'000.0;
        }
    }
#else
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (__get_cpuid_count(0x15, 0, &eax, &ebx, &ecx, &edx)){
        if (eax != 0U && ebx != 0U && ecx != 0U){
            freq = (static_cast<double>(ecx) * ebx) / eax;
        }
    }
    if (freq == 0.0){
        if (__get_cpuid(0x16, &eax, &ebx, &ecx, &edx)){
            const unsigned int mhz = eax;
            if (mhz != 0U){
                freq = static_cast<double>(mhz) * 1'000'000.0;
            }
        }
    }
#endif
    return freq;
}

bool tsc_init(){
    s_tsc_frequency = detect_tsc_frequency_hz();
    if (s_tsc_frequency > 0.0){
        s_tsc_ns_per = 1e9 / s_tsc_frequency;
        return true;
    }
    s_tsc_ns_per = 0.0;
    return false;
}

NSE_ZPP