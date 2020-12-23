#ifndef UTIL_CPUID_H
#define UTIL_CPUID_H

namespace util {
class CPUID {
    private:
        CPUID();

    public:
        static bool isAvxSupported() {
            return gShared.avxSupportted;
        }

    private:
        static CPUID gShared;

    private:
        bool sseSupportted = false;
        bool sse2Supportted = false;
        bool sse3Supportted = false;
        bool ssse3Supportted = false;
        bool sse4_1Supportted = false;
        bool sse4_2Supportted = false;
        bool sse4aSupportted = false;
        bool sse5Supportted = false;
        bool avxSupportted = false;
};
}

#endif
