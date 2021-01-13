/**
 * Thin wrapper around SDL to set up the main window for the app.
 *
 * All the required OpenGL (and helper library) initialization takes place here as well.
 */
#ifndef GUI_MAINWINDOW_H
#define GUI_MAINWINDOW_H

#include <vector>
#include <atomic>
#include <memory>
#include <queue>

struct SDL_Window;
union SDL_Event;

namespace gui {
class GameUI;
class RunLoopStep;

class MainWindow {
    public:
        MainWindow();
        ~MainWindow();

        void show();
        int run();

        void setMouseCaptureState(bool);

        void quit();

        /// Returns a reference to the raw SDL window.
        SDL_Window *getSDLWindow() {
            return this->win;
        }

    private:
        void initGLLibs();
        void configGLContext();

        void makeWindow();

        void handleEvent(const SDL_Event &, int &);
        void restoreWindowSize();
        void saveWindowSize();

        void startFrameFpsUpdate();
        void endFrameFpsUpdate();

    private:
        // default window size
        constexpr static const int kDefaultWidth = 1024, kDefaultHeight = 768;

    private:
        // when set, GL debugging flags are enabled
        bool glDebug = false;

        // main window handle and its associated OpenGL context
        SDL_Window *win = nullptr;
        void *winCtx = nullptr;

        // run the main render loop as long as this is true
        std::atomic_bool running;

        // various rendering pieces
        std::vector<std::shared_ptr<RunLoopStep>> stages;

    private:
        // number of frames for which to average fps
        constexpr static const size_t kNumFrameValues = 20;

        std::shared_ptr<GameUI> gameUi = nullptr;

        std::queue<double> frameTimes;
        std::queue<double> frameTimesTrue;
        size_t framesExecuted = 0;

        double frameTimeAvg = 0.f;
        double frameTimeLast = 0.f;

        double frameTimeAvgTrue = 0.f;
        double frameTimeLastTrue = 0.f;

        size_t frameStartTime = 0;

        bool showProfiler = false;
};
}

#endif
