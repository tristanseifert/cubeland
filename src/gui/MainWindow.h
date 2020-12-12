/**
 * Thin wrapper around SDL to set up the main window for the app.
 *
 * All the required OpenGL (and helper library) initialization takes place here as well.
 */
#ifndef GUI_MAINWINDOW_H
#define GUI_MAINWINDOW_H

#include <atomic>

struct SDL_Window;

namespace gui {
class MainWindow {
    public:
        MainWindow();
        ~MainWindow();

        void show();
        int run();

    private:
        void initGLLibs();
        void configGLContext();

        void makeWindow();

    private:
        // default window size
        constexpr static const int kDefaultWidth = 1024, kDefaultHeight = 768;

    private:
        // when set, GL debugging flags are enabled
        bool glDebug = true;

        // main window handle and its associated OpenGL context
        SDL_Window *win = nullptr;
        void *winCtx = nullptr;

        // run the main render loop as long as this is true
        std::atomic_bool running;
};
}

#endif
