#ifndef GUI_RUNLOOPSTEP_H
#define GUI_RUNLOOPSTEP_H

union SDL_Event;

namespace gui {
class RunLoopStep {
    public:
        virtual ~RunLoopStep() {};

    public:
        virtual void stepAdded() {};
    
        virtual void willBeginFrame() {};

        // immediately before swapping buffers
        virtual void willEndFrame() {};
        // after swapping buffers
        virtual void didEndFrame() {};

        virtual void draw() = 0;

        /// update internal buffers to the new window size
        virtual void reshape(unsigned int width, unsigned int height) {};

        virtual bool handleEvent(const SDL_Event &) = 0;
};
}

#endif
