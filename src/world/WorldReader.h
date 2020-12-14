#ifndef WORLD_WORLDREADER_H
#define WORLD_WORLDREADER_H

namespace world {
/**
 * Interface exported by all world reading implementations.
 *
 * This allows the rest of the game logic to easily operate with worlds read from file, over the
 * network, or other places.
 */
class WorldReader {
    public:
        virtual ~WorldReader() = default;
};
}

#endif
