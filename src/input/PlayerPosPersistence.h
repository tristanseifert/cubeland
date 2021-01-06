/**
 * Provides support for persisting the player's position and camera view angles to the world file.
 * The data is stored under the playerinfo key `player.position`.
 */
#ifndef INPUT_PLAYERPOSPERSISTENCE_H
#define INPUT_PLAYERPOSPERSISTENCE_H

#include <memory>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include "util/ThreadPool.h"

namespace world {
class WorldSource;
}

namespace input {
class InputManager;

class PlayerPosPersistence {
    public:
        PlayerPosPersistence(InputManager *mgr, std::shared_ptr<world::WorldSource> &source);
        ~PlayerPosPersistence();

        void startOfFrame(const glm::vec3 &pos);

        bool loadPosition(glm::vec3 &loadedPos);
        void writePosition();

    private:
        /// Number of ticks between position saves
        constexpr static const size_t kSaveDelayTicks = 300; // 7.5 sec
        /// Player info key for the position data
        static const std::string kDataPlayerInfoKey;

        /// Minimum difference on any view angle to consider dirtying our state
        constexpr static const float kAngleEpsilon = 1.5;
        /// Minimum position difference before dirtying state
        constexpr static const float kPositionEpsilon = 0.2;

    private:
        void tick();

    private:
        /// Encoded player info data for the position and look angles
        struct PlayerPosData {
            glm::vec3 position = glm::vec3(0);
            glm::vec2 cameraAngles = glm::vec2(0);

            template<class Archive> void serialize(Archive &arc) {
                arc(this->position);
                arc(this->cameraAngles);
            }
        };

        using WorkQueue = util::ThreadPool<std::function<void(void)>>;

    private:
        InputManager *input = nullptr;
        std::shared_ptr<world::WorldSource> source = nullptr;

        uint32_t tickHandler = 0;

        size_t dirtyTicks = 0;
        bool dirty = false;

        glm::vec3 lastPosition = glm::vec3(0);
        glm::vec2 lastAngles = glm::vec2(0);

        // all saving happens on this background queue
        WorkQueue saveWorker = WorkQueue("PlayerPos Persistence", 1);
};

}

#endif
