#ifndef SHARED_UTIL_MATH
#define SHARED_UTIL_MATH

/**
 * Integer ceiling function
 */
inline int IntCeil(int numerator, int denominator) {
    return numerator / denominator
             + (((numerator < 0) ^ (denominator > 0)) && (numerator%denominator));
}


#endif
