#ifndef RENDER_CHUNK_WORLDCHUNKDEBUGGER_H
#define RENDER_CHUNK_WORLDCHUNKDEBUGGER_H

#include <cstdint>

namespace render {
class WorldChunk;
};

namespace render::chunk {
class WorldChunkDebugger {
    public:
        WorldChunkDebugger() = delete;
        WorldChunkDebugger(WorldChunk *);

        void draw();

        /// Returns the visibility state of the debugger.
        bool isOpen() const {
            return this->isDebuggerOpen;
        }
        /// Sets the visibility state of the debugger.
        void setOpen(bool open) {
            this->isDebuggerOpen = open;
        }

    private:
        void drawExposureMap();
        void updateExposureMapHighlights();

        struct ExposureMapState {
            // current slice (Y) level of exposure map to show
            int mapY = 0;
            // current row (Z) of exposure map to show
            int mapZ = 0;
            // should that section of the chunk be highlighted?
            bool highlight = false;

            // when set, highlight needs to be updated
            bool updateHighlights = true;
            // current highlight token
            uint64_t highlightId = 0;
        };

        ExposureMapState exposureMapState;

    private:
        void drawHighlightsList();

    private:
        /// chunk we're debugging
        WorldChunk *chunk = nullptr;

        /// whether the debugging window is open
        bool isDebuggerOpen = true;
};
}

#endif
