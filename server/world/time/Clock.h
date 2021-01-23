#ifndef WORLD_TIME_CLOCK_H
#define WORLD_TIME_CLOCK_H

#include <chrono>
#include <string>

#include <cpptime.h>
#include <cereal/access.hpp>

namespace world {
class WorldSource;

/**
 * Serves as the source of the current time for the world.
 */
class Clock {
    public:
        Clock(WorldSource *source);
        ~Clock();

        void resume();
        void stop();

        double getTime() const {
            return this->currentTime;
        }

    private:
        // saved in world data containing the time
        struct TimeData {
            double time;

            private:
                friend class cereal::access;
                template <class Archive> void serialize(Archive &ar) {
                    ar(this->time);
                }
        };

        /// interval at which to update time, in ms
        constexpr static const size_t kUpdateInterval = 100;
        /// world info key for time storage
        static const std::string kTimeInfoKey;

    private:
        void step();

        void loadTime();
        void saveTime();

    private:
        double currentTime = 0.;
        double tickStep = 0.;

        CppTime::Timer timer;
        bool isPaused = true;
        CppTime::timer_id updateTimer;

        WorldSource *source = nullptr;

        /// time of the last tick
        std::chrono::steady_clock::time_point lastStep;
};
}

#endif
