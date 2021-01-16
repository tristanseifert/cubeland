/**
 * Helper methods to implement non-linear easing functions.
 */
#ifndef UTIL_EASING_H
#define UTIL_EASING_H

#include <cmath>

namespace util {
class Easing {
    public:
        static double easeInSine(const double t) {
            return sin(M_PI * t);
        }

        static double easeOutSine(const double t) {
            return 1 + sin(M_PI_2 * (t - 1));
        }

        static double easeInOutSine(const double t) {
            return 0.5 * (1 + sin(M_PI * (t - 0.5)));
        }

        static double easeInQuad(const double t) {
            return t * t;
        }

        static double easeOutQuad(const double t) { 
            return t * (2 - t);
        }

        static double easeInOutQuad(const double t) {
            if(t < 0.5) {
                return 2 * t * t;
            } else {
                return (-2 * t * t) + (4 * t) - 1;
            }
        }

        static double easeInCubic(const double t) {
            return t * t * t;
        }

        static double easeOutCubic(const double t) {
            const auto f = (t - 1);
            return f * f * f + 1;
        }

        static double easeInOutCubic(const double t) {
            if(t < 0.5) {
                return 4 * t * t * t;
            } else {
                auto f = ((2 * t) - 2);
                return 0.5 * f * f * f + 1;
            }
        }

        static double easeInQuart(const double t) {
            const auto p = t * t;
            return p * p;
        }

        static double easeOutQuart(const double t) {
            const auto f = (t - 1);
            return f * f * f * (1 - t) + 1; 
        }

        static double easeInOutQuart(const double t) {
            if(t < 0.5) {
                return 8 * t * t * t * t;
            } else {
                const auto f = (t - 1);
                return -8 * f * f * f * f + 1;
            }
        }

        static double easeInQuint(const double t) {
            double t2 = t * t;
            return t * t2 * t2;
        }

        static double easeOutQuint(const double t) {
            const auto f = (t - 1);
            return f * f * f * f * f + 1;
        }

        static double easeInOutQuint(const double t) {
            if(t < 0.5) {
                return 16 * t * t * t * t * t;
            } else {
                const auto f = ((2 * t) - 2);
                return  0.5 * f * f * f * f * f + 1;
            }
        }

        static double easeInExpo(const double t) {
            return (pow(2, 8 * t) - 1) / 255;
        }

        static double easeOutExpo(const double t) {
            return 1 - pow(2, -8 * t);
        }

        static double easeInOutExpo(const double t) {
            if(t < 0.5) {
                return (pow( 2, 16 * t) - 1) / 510;
            } else {
                return 1 - 0.5 * pow(2, -16 * (t - 0.5));
            }
        }

        static double easeInCirc(const double t) {
            return 1 - sqrt( 1 - t);
        }

        static double easeOutCirc(const double t) {
            return sqrt(t);
        }

        static double easeInOutCirc(const double t) {
            if(t < 0.5) {
                return (1 - sqrt(1 - 2 * t)) * 0.5;
            } else {
                return (1 + sqrt(2 * t - 1)) * 0.5;
            }
        }

        static double easeInBack(const double t) {
            return t * t * (2.70158 * t - 1.70158);
        }

        static double easeOutBack(const double t) {
            const auto f = (1 - t);
            return 1 - (f * f * f - f * sin(f * M_PI));
        }

        static double easeInOutBack(const double t) {
            if(t < 0.5) {
                const auto f = 2 * t;
                return 0.5 * (f * f * f - f * sin(f * M_PI));
            } else {
                const auto f = (1 - (2*t - 1));
                return 0.5 * (1 - (f * f * f - f * sin(f * M_PI))) + 0.5;
            }
        }

        static double easeInElastic(const double t) {
            double t2 = t * t;
            return t2 * t2 * sin(t * M_PI * 4.5);
        }

        static double easeOutElastic(const double t) {
            double t2 = (t - 1) * (t - 1);
            return 1 - t2 * t2 * cos(t * M_PI * 4.5);
        }

        static double easeInOutElastic(const double t) {
            double t2;
            if(t < 0.45) {
                t2 = t * t;
                return 8 * t2 * t2 * sin( t * M_PI * 9  );
            } else if(t < 0.55) {
                return 0.5 + 0.75 * sin( t * M_PI * 4  );
            } else {
                t2 = (t - 1) * (t - 1);
                return 1 - 8 * t2 * t2 * sin( t * M_PI * 9  );
            }
        }

        static double easeInBounce(const double t) {
            return pow(2, 6 * (t - 1)) * abs(sin(t * M_PI * 3.5));
        }

        static double easeOutBounce(const double t) {
            return 1 - pow(2, -6 * t) * abs(cos(t * M_PI * 3.5));
        }

        static double easeInOutBounce(const double t) {
            if(t < 0.5) {
                return 8 * pow(2, 8 * (t - 1)) * abs(sin(t * M_PI * 7));
            } else {
                return 1 - 8 * pow(2, -8 * t) * abs(sin(t * M_PI * 7));
            }
        }
};
}

#endif
