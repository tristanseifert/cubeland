#include "MainWindow.h"
#include "GameUI.h"

#include "io/PrefsManager.h"

#include <Logging.h>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

#include <SDL.h>

using namespace gl;
using namespace gui;

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Initializes the window.
 */
MainWindow::MainWindow() {
    this->configGLContext();
    this->makeWindow();

    // create the renderers
    this->ui = std::make_shared<GameUI>(this->win, this->winCtx);

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
    this->ui = nullptr;

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
    XASSERT(this->win, "Window must exist");

    SDL_ShowWindow(this->win);
}

/**
 * Runs the window event loop.
 *
 * @return Reason the window closed.
 */
int MainWindow::run() {
    int reason = 0;
    SDL_Event event;

    Logging::trace("Entering main loop");

    // main run loop
    while(this->running) {
        // handle events
        while (SDL_PollEvent(&event)) {
            // ignore event if handled by GUI layer
            if(this->ui->handleEvent(event)) {
                continue;
            }

            // otherwise, check the event out
            switch(event.type) {
                // quit
                case SDL_QUIT:
                    this->running = false;
                    reason = 1;
                    break;

                // unhandled event type
                default:
                    break;
            }
        }

        // prepare renderers
        this->ui->willBeginFrame();

        // clear the output buffer, then draw the scene and UI ontop
        glClear(GL_COLOR_BUFFER_BIT);

        this->ui->draw();

        // swap buffers (this will synchronize to vblank if enabled)
        SDL_GL_SwapWindow(this->win);
    }

    // clean-up
    this->saveWindowSize();

    return reason;
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
