#include "Terrain.h"

#include "world/chunk/Chunk.h"
#include "world/chunk/ChunkSlice.h"

#include <world/block/BlockIds.h>

#include <Logging.h>
#include <FastNoise/FastNoise.h>
#include <glm/vec3.hpp>
#include <uuid.h>

#if PROFILE
#include <mutils/time/profiler.h>
#else
#define PROFILE_SCOPE(x) 
#define LOCK_GUARD(lock, name) std::lock_guard<std::mutex> lg(lock)
#endif

using namespace world;

/**
 * This is the encoded noise tree, as output by the FastNoise NoiseTool application.
 */
static const char *kNodeTree = "EgACAAAAAAAgQBEAAAAAQBoAFADD9Sg/DQAEAAAAAAAgQAkAAAAAAD8BBAAAAAAAAABAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADNzEw+AJqZGT8=";

/**
 * Translates a FastNoise SIMD level to string.
 */
const char *GetSIMDLevelName(FastSIMD::eLevel lvl) {
    switch(lvl) {
        default:
        case FastSIMD::Level_Null:   return "NULL";
        case FastSIMD::Level_Scalar: return "Scalar";
        case FastSIMD::Level_SSE:    return "SSE";
        case FastSIMD::Level_SSE2:   return "SSE2";
        case FastSIMD::Level_SSE3:   return "SSE3";
        case FastSIMD::Level_SSSE3:  return "SSSE3";
        case FastSIMD::Level_SSE41:  return "SSE4.1";
        case FastSIMD::Level_SSE42:  return "SSE4.2";
        case FastSIMD::Level_AVX:    return "AVX";
        case FastSIMD::Level_AVX2:   return "AVX2";
        case FastSIMD::Level_AVX512: return "AVX512";
        case FastSIMD::Level_NEON:   return "NEON";
    }
}

/**
 * Instantiates the FastNoise node graph.
 */
Terrain::Terrain(int32_t _seed) : seed(_seed) {
    this->generator = FastNoise::NewFromEncodedNodeTree(kNodeTree);
    Logging::info("Terrain generator SIMD level: {} (seed ${:x})", GetSIMDLevelName(this->generator->GetSIMDLevel()), this->seed);
}

/**
 * Generates a new chunk of terrain data.
 */
std::shared_ptr<Chunk> Terrain::generateChunk(int x, int z) {
    glm::ivec3 worldPos(x*256, 0, z*256);

    // generate a 256x256x256 noise buffer
    std::vector<float> noise;
    FastNoise::OutputMinMax noiseRange;
    noise.resize(256*256*256);

    {
        PROFILE_SCOPE(GenerateNoise);
        noiseRange = this->generator->GenUniformGrid3D(noise.data(), worldPos.x, worldPos.y,
                worldPos.z, 256, 256, 256, this->frequency, this->seed);
        // Logging::info("Noise range: [{}, {}]", noiseRange.min, noiseRange.max);
    }
    // allocate a chunk and fill it
    auto chunk = std::make_shared<Chunk>();
    chunk->worldPos = glm::ivec2(x, z);
    this->prepareChunkMeta(chunk);

    this->fillFloor(chunk);

    {
        PROFILE_SCOPE(FillSlices);
        for(size_t y = 1; y < this->maxHeight; y++) {
            this->fillSlice(noise, y, chunk);
        }
    }

    // done :D
    return chunk;
}

/**
 * Prepares a chunk's metadata and type maps.
 */
void Terrain::prepareChunkMeta(std::shared_ptr<Chunk> chunk) {
    // write generator ID
    chunk->meta["me.tseifert.cubeland.generator"] = "world::Terrain::v1";
    chunk->meta["me.tseifert.cubeland.generator.seed"] = this->seed;

    // type map
    static const size_t kNumBlockIds = 3;
    static const uuids::uuid kBlockIds[kNumBlockIds] = {
        // 0: air
        world::kAirBlockId,
        // 1: stone
        world::blocks::kStoneBlockId,
        // 2: dirt
        world::blocks::kDirtBlockId,
    };

    // build the slice ID -> UUID map
    ChunkRowBlockTypeMap idMap;
    for(size_t i = 0; i < kNumBlockIds; i++) {
        idMap.idMap[i] = kBlockIds[i];
    }
    chunk->sliceIdMaps.push_back(idMap);
}

/**
 * Writes a solid ground floor at y=0. It is made of stone.
 */
void Terrain::fillFloor(std::shared_ptr<Chunk> chunk) {
    auto slice = new ChunkSlice;

    for(size_t z = 0; z < 256; z++) {
        auto row = chunk->allocRowSparse();
        row->defaultBlockId = 1;
        row->typeMap = 0;

        slice->rows[z] = row;
    }

    chunk->slices[0] = slice;
}

/**
 * Populates the given y level of the chunk, allocating the slice as needed.
 */
void Terrain::fillSlice(const std::vector<float> &noise, const size_t y, std::shared_ptr<Chunk> chunk) {
    // PROFILE_SCOPE(FillSlice);

    const size_t yOffset = (y * 256);

    // allocate a slice
    bool written = false;
    auto slice = new ChunkSlice;

    // iterate for each row
    for(size_t z = 0; z < 256; z++) {
        const size_t zOffset = yOffset + (z * 256 * 256);

        // check to see if we want a sparse row by counting the number of filled in blocks
        size_t numWritten = 0;
        for(size_t x = 0; x < 256; x++) {
            const float value = noise[zOffset+x];

            if(value <= this->surfaceLevel) {
                numWritten++;
            }
        }
        // skip if not a single block is solid
        if(!numWritten) continue;

        // we will want a sparse row
        ChunkSliceRow *row = nullptr;
        bool isSparse = true;
        bool rowWritten = false;

        if(numWritten < ChunkSliceRowSparse::kMaxEntries) {
            auto r = chunk->allocRowSparse();
            r->defaultBlockId = 0;

            row = r;
        } else {
            row = chunk->allocRowDense();
            isSparse = false;
        }
        row->typeMap = 0;

        // then, iterate for each column inside it
        for(size_t x = 0; x < 256; x++) {
            const size_t noiseIdx = zOffset + x;
            const float value = noise[noiseIdx];

            if(value <= this->surfaceLevel) {
                // fill with dirt
                row->set(x, 2);
                rowWritten = true;
            } else {
                if(!isSparse) {
                    row->set(x, 0);
                    rowWritten = true;
                }
            }
        }

        // store the row if it was written
        if(rowWritten) {
            row->prepare();
            slice->rows[z] = row;
            written = true;
        } else {
            // TODO: mark row as unused
        }
    }

    // add it to the chunk if the slice was written
    if(written) {
        chunk->slices[y] = slice;
    } else {
        delete slice;
    }
}

