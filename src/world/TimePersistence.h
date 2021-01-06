#ifndef WORLD_TIMEPERSISTENCE_H
#define WORLD_TIMEPERSISTENCE_H

#include <string>
#include <memory>
#include <cstddef>

namespace world {
class WorldSource;

class TimePersistence {
    public:
        TimePersistence(std::shared_ptr<WorldSource> &source, double *timePtr);
        ~TimePersistence();

    private:
        void saveTime();
        void restoreTime();

        void tick();

        bool load();
        void save();

    private:
        /// ticks between saving of time
        constexpr static const size_t kSaveInterval = 250;
        /// player info key for world time
        static const std::string kDataPlayerInfoKey;

    private:
        /// struct to serialize to/from the world file containing current time
        struct TimeInfo {
            double time;

            template<class Archive> void serialize(Archive &arc) {
                arc(this->time);
            }
        };

    private:
        /// tick callback token
        uint32_t tickCallback = 0;
        /// number of ticks since the time was last saved
        size_t ticksSinceSave = 0;

        /// world source into which the time is written
        std::shared_ptr<WorldSource> source = nullptr;
        /// time struct to modify
        double *time = nullptr;
};

}

#endif
