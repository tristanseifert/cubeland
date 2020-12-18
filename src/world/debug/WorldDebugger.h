/**
 * Provides a graphical debugger for working with the world reader system.
 *
 * As the game works, this debugger would get a reference to whatever world is being displayed on
 * the screen at the time.
 */
#ifndef WORLD_DEBUG_WORLDDEBUGGER_H
#define WORLD_DEBUG_WORLDDEBUGGER_H

#include "gui/GameWindow.h"

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <variant>

#include <blockingconcurrentqueue.h>

namespace gui {
class GameUI;
}

namespace world {
class WorldReader;
class FileWorldReader;

struct Chunk;
struct ChunkSliceRowSparse;
struct ChunkSliceRowDense;

class WorldDebugger: public gui::GameWindow {
    public:
        WorldDebugger();
        ~WorldDebugger();

        void draw(gui::GameUI *) override;

        /// Returns the visibility state of the debugger.
        bool isOpen() const {
            return this->isDebuggerOpen;
        }
        /// Sets the visibility state of the debugger.
        void setOpen(bool open) {
            this->isDebuggerOpen = open;
        }
        /// Sets the world displayed in the debugger.
        void setWorld(std::shared_ptr<WorldReader> newWorld) {
            this->world = newWorld;
        }

    private:
        void loadWorldInfo();

        void drawChunkUi(gui::GameUI *);
        void drawChunkReadUi(gui::GameUI *);
        void drawChunkWriteUi(gui::GameUI *);

        void drawFileWorldUi(gui::GameUI *, std::shared_ptr<FileWorldReader>);
        void drawFileTypeMap(gui::GameUI *, std::shared_ptr<FileWorldReader>);

        void drawChunkViewer(gui::GameUI *);
        void drawChunkMeta(gui::GameUI *);
        void drawChunkIdMap(gui::GameUI *);
        void drawChunkRows(gui::GameUI *);
        void drawRowInfo(gui::GameUI *, std::shared_ptr<ChunkSliceRowSparse>);
        void drawRowInfo(gui::GameUI *, std::shared_ptr<ChunkSliceRowDense>);
        void drawBlockInfo(gui::GameUI *);
        void resetChunkViewer();

        void printMetaValue(const std::variant<std::monostate, bool, std::string, double, int64_t> &val);

    private:
        void workerMain();
        void sendWorkerNop();

        void prepareChunkMaps(std::shared_ptr<Chunk> chunk);
        void fillChunkSolid(std::shared_ptr<Chunk> chunk, size_t yMax);
        void fillChunkSphere(std::shared_ptr<Chunk> chunk, size_t radius = 32);

    private:
        /// Whether the debug window is open
        bool isDebuggerOpen = true;
        /// Whether the chunk viewer window is open
        bool isChunkViewerOpen = false;

        /// World reader currently in use
        std::shared_ptr<WorldReader> world = nullptr;
        /// Error from opening the world, if any
        std::unique_ptr<std::string> worldError = nullptr;

        /// Chunk to show in the viewer, if any
        std::shared_ptr<Chunk> chunk = nullptr;

        /// if set, we show the busy indicator
        bool isBusy = false;
        /// what exactly we're busy with
        std::string busyText = "nothing";

        /// chunk UI state
        struct {
            /// X/Z coord for the block to write
            int writeCoord[2] = {0, 0};
            /// current fill type
            int fillType = 1;
            /// fill level
            int fillLevel = 32;
            /// whether block properties are written
            bool writeBlockProps = true;

            /// X/Z coord for the block to read
            int readCoord[2] = {0, 0};
        } chunkState;

        /// chunk viewer state
        struct ChunkViewerState {
            // currently selected slice ID map
            int currentIdMap;
            // currently selected slice
            int currentSlice;
            // currently selected row
            int currentRow;
        };
        ChunkViewerState viewerState;

    private:
        /// worker thread processes requests as long as this is set
        std::atomic_bool workerRun;
        /// worker thread 
        std::unique_ptr<std::thread> worker = nullptr;
        /// work requests sent to the thread
        moodycamel::BlockingConcurrentQueue<std::function<void(void)>> workQueue;
};
}

#endif
