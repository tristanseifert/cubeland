#ifndef RENDER_WORLDRENDERDEBUGGER_H
#define RENDER_WORLDRENDERDEBUGGER_H

namespace render {
class WorldRenderer;

class WorldRendererDebugger {
    public:
        WorldRendererDebugger() = delete;
        WorldRendererDebugger(WorldRenderer *renderer);

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
        void drawFovUi();
        void drawStepsTable();

    private:
        bool isDebuggerOpen = true;

        WorldRenderer *renderer = nullptr;
};
}

#endif
