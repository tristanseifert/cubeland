#ifndef WORLD_WORLDGENERATOR_H
#define WORLD_WORLDGENERATOR_H

#include <future>
#include <memory>

namespace world {
struct Chunk;

/**
 * Provides the interface that all world generators implement.
 *
 * Note that unlike the world readers, these functions run synchronously. This is because they
 * will usually only ever be executed in the context of the world source, which has its own thread
 * pool.
 */
class WorldGenerator {
    public:
        virtual ~WorldGenerator() = default;

    public:
        /**
         * Generates data for a chunk at the given world coordinate.
         */
        virtual std::promise<std::shared_ptr<Chunk>> generateChunk(int x, int z) = 0;
};
}

#endif
