#include "CPUID.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

using namespace util;

CPUID CPUID::gShared;

#ifdef __GNUC__

void __cpuid(int* cpuinfo, int info) {
    __asm__ __volatile__(
        "xchg %%ebx, %%edi;"
        "cpuid;"
        "xchg %%ebx, %%edi;"
        :"=a" (cpuinfo[0]), "=D" (cpuinfo[1]), "=c" (cpuinfo[2]), "=d" (cpuinfo[3])
        :"0" (info)
    );
}

unsigned long long _xgetbv(unsigned int index) {
    unsigned int eax, edx;
    __asm__ __volatile__(
        "xgetbv;"
        : "=a" (eax), "=d"(edx)
        : "c" (index)
    );
    return ((unsigned long long)edx << 32) | eax;
}
#endif


/**
 * Initializes the CPUID detector.
 *
 * This will check all available extensions.
 */
CPUID::CPUID() {
    // get cpuid
    int cpuinfo[4];
    __cpuid(cpuinfo, 1);

    // SSE through SSE 4.2 extensions
    this->sseSupportted= cpuinfo[3] & (1 << 25) || false;
    this->sse2Supportted= cpuinfo[3] & (1 << 26) || false;
    this->sse3Supportted= cpuinfo[2] & (1 << 0) || false;
    this->ssse3Supportted= cpuinfo[2] & (1 << 9) || false;
    this->sse4_1Supportted= cpuinfo[2] & (1 << 19) || false;
    this->sse4_2Supportted= cpuinfo[2] & (1 << 20) || false;

    // check AVX extensions
    this->avxSupportted = cpuinfo[2] & (1 << 28) || false;
    bool osxsaveSupported = cpuinfo[2] & (1 << 27) || false;
    if (osxsaveSupported && this->avxSupportted) {
        // _XCR_XFEATURE_ENABLED_MASK = 0
        unsigned long long xcrFeatureMask = _xgetbv(0);
        this->avxSupportted = (xcrFeatureMask & 0x6) == 0x6;
    }

    /// SSE 4a, SSE 5
    __cpuid(cpuinfo, 0x80000000);
    int numExtendedIds = cpuinfo[0];
    if (numExtendedIds >= 0x80000001) {
        __cpuid(cpuinfo, 0x80000001);
        this->sse4aSupportted = cpuinfo[2] & (1 << 6) || false;
        this->sse5Supportted = cpuinfo[2] & (1 << 11) || false;
    }
}

