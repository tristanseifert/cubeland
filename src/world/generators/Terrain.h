/**
 * Basic terrain generation using noise.
 */
#ifndef WORLD_GENERATORS_TERRAIN_H
#define WORLD_GENERATORS_TERRAIN_H

#include "world/WorldGenerator.h"

#include <memory>

#include <FastNoise/FastNoise.h>

namespace world {
struct Chunk;

class Terrain: public WorldGenerator {
    public:
        Terrain(int32_t seed = 420);

        virtual std::shared_ptr<Chunk> generateChunk(int x, int z);

    private:
        void prepareChunkMeta(std::shared_ptr<Chunk> chunk);
        void fillFloor(std::shared_ptr<Chunk> chunk);
        void fillSlice(const std::vector<float> &noise, const size_t y, std::shared_ptr<Chunk> chunk);

    private:
        // noise generator
        FastNoise::SmartNode<> generator;
        // noise frequency
        float frequency = 0.005;
        // surface level
        float surfaceLevel = -0.069;
        // maximum height of generated structures
        size_t maxHeight = 120;
        // seed used for world generation
        int32_t seed;
};
}

#endif
