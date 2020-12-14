#include "MainWindow.h"
#include "GameUI.h"
#include "render/WorldRenderer.h"

#include "io/PrefsManager.h"

#include <Logging.h>

#include <mutils/time/profiler.h>
#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>
#include <SDL.h>

#include <iterator>

using namespace gl;
using namespace gui;

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Initializes the window.
 */
MainWindow::MainWindow() {
    int w = 0, h = 0;

    this->configGLContext();
    this->makeWindow();

    // set up profiling
    MUtils::Profiler::Init();
    PROFILE_NAME_THREAD("Main");

    // create the renderers
    auto world = std::make_shared<render::WorldRenderer>(this);
    this->stages.push_back(world);

    auto ui = std::make_shared<GameUI>(this->win, this->winCtx);
    this->stages.push_back(ui);

    // initialize renderers with current viewport size
    SDL_GL_GetDrawableSize(this->win, &w, &h);
    glViewport(0, 0, (GLsizei) w, (GLsizei) h);

    for(auto &render : this->stages) {
        render->reshape(w, h);
    }

    this->running = true;
}

/**
 * Sets up GL helper libraries.
 *
 * This is performed immediately after the first GL context is created.
 */
void MainWindow::initGLLibs() {
    glbinding::Binding::initialize();
}

/**
 * Configures the SDL options for the context.
 *
 * We ask for at least a OpenGL 4.0 core context. Double buffering is requested.
 */
void MainWindow::configGLContext() {
    // opengl forawrd debug context flags
    const auto ctxFlags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG | 
                          (this->glDebug ? SDL_GL_CONTEXT_DEBUG_FLAG : 0);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, ctxFlags);

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0); // no depth required
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}

/**
 * Creates the SDL window and its OpenGL context.
 */
void MainWindow::makeWindow() {
    int err;

    // create window; allowing for HiDPI contexts
    const auto flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN | 
                       SDL_WINDOW_RESIZABLE;
    this->win = SDL_CreateWindow("Cubeland", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            kDefaultWidth, kDefaultHeight, flags);

    XASSERT(this->win != nullptr, "Failed to create window: {}", SDL_GetError());

    // resize window to stored size if needed
    if(io::PrefsManager::getBool("window.restoreSize")) {
        this->restoreWindowSize();
    }

    // create the GL context and initialize our GL libs
    this->winCtx = SDL_GL_CreateContext(this->win);
    XASSERT(this->winCtx != nullptr, "Failed to create OpenGL context: {}", SDL_GetError());

    SDL_GL_MakeCurrent(this->win, this->winCtx);

    this->initGLLibs();

    // enable VSync if possible
    err = SDL_GL_SetSwapInterval(1);
    if(err) {
        Logging::error("Failed to enable vsync ({}): {}", err, SDL_GetError());
    }

    // set some context defaults
    glClearColor(0, 0, 0, 1);

    // log info on the GL version
    Logging::info("GL version {}; vendor {}, renderer {}", glGetString(GL_VERSION),
                  glGetString(GL_VENDOR), glGetString(GL_RENDERER));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Releases all SDL resources.
 */
MainWindow::~MainWindow() {
    // get rid of renderers
    this->stages.clear();

    // destroy window and context
    if(this->winCtx) {
        SDL_GL_DeleteContext(this->winCtx);
        this->winCtx = nullptr;
    }
    if(this->win) {
        SDL_DestroyWindow(this->win);
        this->win = nullptr;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Makes the window visible.
 */
void MainWindow::show() {
    int w, h;

    XASSERT(this->win, "Window must exist");

    // capture mouse
    // SDL_SetRelativeMouseMode(SDL_TRUE);
    // SDL_ShowCursor(1);

    /**
     * MacOS kludge: we need to give SDL an extra hint to make relative mouse
     * mode work. (see https://forums.libsdl.org/viewtopic.php?p=50057)
     */
#if MACOS
    Logging::info("Using relative mouse mode warp kludge");
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
#endif

    SDL_ShowWindow(this->win);
}

/**
 * Sets the mouse capture state.
 */
void MainWindow::setMouseCaptureState(bool captured) {
    if(captured) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_ShowCursor(1);
    } else {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(0);
    }
}

/**
 * Runs the window event loop.
 *
 * @return Reason the window closed.
 */
int MainWindow::run() {
    int w, h;
    int reason = 0;
    SDL_Event event;

    Logging::trace("Entering main loop");

    // main run loop
    while(this->running) {
        // start the FPS counting
        MUtils::Profiler::NewFrame();
        this->startFrameFpsUpdate();

        // handle events
        {
            PROFILE_SCOPE(HandleEvents);
            while (SDL_PollEvent(&event)) {
                this->handleEvent(event, reason);
            }
        }

        // prepare renderers
        {
            PROFILE_SCOPE(WillBeginFrame);
            for(auto rit = this->stages.rbegin(); rit != this->stages.rend(); ++rit) {
                auto &render = *rit;
                render->willBeginFrame();
            }
        }

        // clear the output buffer, then draw the scene and UI ontop
        {
            PROFILE_SCOPE(Draw);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            SDL_GL_GetDrawableSize(this->win, &w, &h);
            glViewport(0, 0, (GLsizei) w, (GLsizei) h);

            for(auto &render : this->stages) {
                render->draw();
            }
        }

        // swap buffers (this will synchronize to vblank if enabled)
        {
            PROFILE_SCOPE(WillEndFrame);
            for(auto &render : this->stages) {
                render->willEndFrame();
            }
        }

        this->endFrameFpsUpdate();
        {
            PROFILE_SCOPE(SwapWindow);
            SDL_GL_SwapWindow(this->win);
        }

        // invoke the final frame lifecycle callback and set up for the next one
        PROFILE_SCOPE(DidEndFrame);
        for(auto &render : this->stages) {
            render->didEndFrame();
        }
    }

    return reason;
}

/**
 * Handles events provided by SDL.
 */
void MainWindow::handleEvent(const SDL_Event &event, int &reason) {
    int w, h;

    // always handle some events
    switch(event.type) {
        // window events
        case SDL_WINDOWEVENT: {
            // ignore events for other windows
            auto ourId = SDL_GetWindowID(this->win);
            if(event.window.windowID != ourId) {
                break;
            }

            switch (event.window.event) {
                // window resized; reshape renderers
                case SDL_WINDOWEVENT_RESIZED: {
                    PROFILE_SCOPE(Reshape);

                    // update viewport
                    SDL_GL_GetDrawableSize(this->win, &w, &h);
                    glViewport(0, 0, (GLsizei) w, (GLsizei) h);

                    this->saveWindowSize();

                    for(auto &render : this->stages) {
                        render->reshape(w, h);
                    }
                    break;
                }

                // we ignore all other window events
                default:
                    break;
            }
            break;
        }

        // quit
        case SDL_QUIT:
            this->running = false;
            reason = 1;
            break;
    }

    // provide events to the UI stages in reverse order
    PROFILE_SCOPE(StageEvent);
    for(auto rit = this->stages.rbegin(); rit != this->stages.rend(); ++rit) {
        auto &render = *rit;

        if(render->handleEvent(event)) {
            return;
        }
    }
}

/**
 * Restores the window size.
 */
void MainWindow::restoreWindowSize() {
    int w = io::PrefsManager::getUnsigned("window.width", kDefaultWidth);
    int h = io::PrefsManager::getUnsigned("window.height", kDefaultHeight);
    XASSERT(w > 0 && h > 0, "Invalid window size (w {}, h {})", w, h);

    SDL_SetWindowSize(this->win, w, h);
    SDL_SetWindowPosition(this->win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}
/**
 * Gets the size of the window and writes it to the preferences.
 */
void MainWindow::saveWindowSize() {
    int w = 0, h = 0;

    SDL_GetWindowSize(this->win, &w, &h);
    XASSERT(w > 0 && h > 0, "Invalid window size (w {}, h {})", w, h);

    io::PrefsManager::setUnsigned("window.width", w);
    io::PrefsManager::setUnsigned("window.height", h);
}



/**
 * Start of frame handler for fps counting
 */
void MainWindow::startFrameFpsUpdate() {
    this->frameStartTime = (unsigned long) SDL_GetPerformanceCounter();
}

/**
 * Calculates the length of the frame, and if enough samples have been acquired, calculates the
 * average time needed to render a frame.
 */
void MainWindow::endFrameFpsUpdate() {
    // Calculate the difference in milliseconds
    unsigned long now = (unsigned long) SDL_GetPerformanceCounter();
    unsigned long difference = now - this->frameStartTime;

    double frequency = (double) SDL_GetPerformanceFrequency();
    this->frameTimeLast = (((double) difference) / frequency) * 1000.f;

    // Push it into the averaging queue.
    this->frameTimes.push(this->frameTimeLast);

    // If we have enough entries, average.
    if(this->frameTimes.size() == kNumFrameValues) {
        // calculate an average
        this->frameTimeAvg = 0.f;

        for(size_t i = 0; i < kNumFrameValues; i++) {
            this->frameTimeAvg += this->frameTimes.front();
            this->frameTimes.pop();
        }

        this->frameTimeAvg /= (double) kNumFrameValues;
    }

    // increment frame counter
    this->framesExecuted++;
}
